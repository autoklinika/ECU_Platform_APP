#include "ecu/app/runtime/runtime.hpp"
#include <limits>
#include <stdexcept>

namespace ecu::app::runtime {
namespace {
using sdk::Code;
using sdk::RuntimeAction;
using sdk::RuntimeState;
namespace ct = core::transport;
namespace cr = core::runtime;
void increment(std::uint64_t& value, std::uint64_t by = 1) noexcept {
  const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
  value = by > maximum - value ? maximum : value + by;
}
}
Runtime::Runtime(ct::ICanDriver& driver, const core::time::IMonotonicClock& clock,
                 cr::ResourceManager& resources, Config config)
    : driver_(driver), clock_(clock), resources_(resources), config_(config),
      operation_timer_(clock), service_timer_(clock), sink_(*this) {
  const auto props = clock.properties();
  if (!core::time::is_valid_clock_properties(props) ||
      !ct::is_valid_can_channel_config(config.channel) ||
      config.channel.timestamp_domain != props.domain ||
      config.channel.mode != ct::CanMode::normal || config.channel.fd_enabled ||
      config.resource.resource_class != cr::ResourceClass::can_channel ||
      config.request_id > 0x7ff || config.response_id > 0x7ff ||
      config.request_id == config.response_id || config.rx_budget == 0 ||
      config.rx_budget > 64 || config.operation_timeout.count() <= 0 ||
      config.service_timeout.count() <= 0)
    throw std::invalid_argument("invalid runtime composition");
}
Runtime::~Runtime() { shutdown(); }
void Runtime::touch() noexcept { increment(state_.revision); }
sdk::RuntimeSnapshot Runtime::snapshot() const noexcept { return state_; }
Code Runtime::submit(const sdk::RuntimeRequest& r) noexcept {
  if (servicing_ || pending_ || completion_) return Code::busy;
  if (state_.state == RuntimeState::stopped) return Code::invalid_state;
  if (r.activation == 0 || r.operation == 0 || r.action == RuntimeAction::none)
    return Code::invalid_input;
  if (r.action == RuntimeAction::connect) {
    if (r.session != 0) return Code::stale_session;
    if (state_.state == RuntimeState::faulted) return Code::invalid_state;
    if (state_.state != RuntimeState::idle) return Code::resource_busy;
  } else {
    if (state_.state == RuntimeState::idle) return Code::invalid_state;
    if (r.activation != state_.owner_activation) return Code::permission_denied;
    if (r.session == 0 || r.session != state_.session) return Code::stale_session;
    if (r.action == RuntimeAction::recover) {
      if (state_.state != RuntimeState::faulted) return Code::invalid_state;
    } else if (r.action != RuntimeAction::sample && r.action != RuntimeAction::disconnect) {
      return Code::invalid_input;
    } else if (state_.state != RuntimeState::ready) return Code::invalid_state;
  }
  const auto began = cancellation_.begin();
  if (began.status != cr::CancellationBeginStatus::started) return Code::counter_exhausted;
  token_ = began.token;
  request_ = r;
  pending_ = true; sent_ = false; matched_ = false;
  state_.pending_operation = r.operation;
  state_.last_code = Code::accepted;
  touch();
  // Covers time waiting for the first executor tick as well as physical reply.
  if (r.action != RuntimeAction::disconnect && r.action != RuntimeAction::recover &&
      operation_timer_.arm(config_.operation_timeout) != core::safety::DeadlineWatchdogStatus::ok) {
    complete(Code::clock_fault);
  }
  return Code::accepted;
}
Code Runtime::cancel(const std::uint64_t activation, const std::uint64_t operation) noexcept {
  if (servicing_) return Code::busy;
  if (!pending_) return Code::not_found;
  if (activation != request_.activation) return Code::permission_denied;
  if (operation == 0 || operation != request_.operation) return Code::stale_activation;
  const auto status = cancellation_.request(token_);
  return status == cr::CancellationRequestStatus::requested ||
         status == cr::CancellationRequestStatus::already_requested ? Code::accepted : Code::invalid_state;
}
void Runtime::complete(const Code code, const bool observed) noexcept {
  if (!pending_) return;
  completion_ = sdk::RuntimeCompletion{request_, code, state_.session,
                                      observed, observed ? matched_at_.count() : 0};
  static_cast<void>(cancellation_.complete(token_));
  operation_timer_.disarm(); token_ = {};
  pending_ = false; sent_ = false; matched_ = false;
  state_.pending_operation = 0;
  state_.last_code = code;
  touch();
}
bool Runtime::take_completion(sdk::RuntimeCompletion& result) noexcept {
  if (servicing_ || !completion_) return false;
  result = *completion_; completion_.reset(); return true;
}
void Runtime::close_bus() noexcept {
  // Core stop retains the physical lease; terminal destruction releases it.
  // In Core fault state stop is a no-op; destruction still closes the driver.
  if (bus_) { bus_->stop(); bus_.reset(); }
  service_timer_.disarm();
}
void Runtime::release() noexcept {
  if (lease_.valid()) {
    const auto released = resources_.release(lease_);
    if (released != cr::ResourceReleaseStatus::released) {
      state_.state = RuntimeState::faulted;
      state_.last_code = Code::cleanup_failed;
      state_.resources_held = resources_.owns(lease_);
      touch(); return;
    }
    lease_ = {};
  }
  state_.resources_held = false;
}
void Runtime::fault(const Code code) noexcept {
  close_bus();
  // Quarantine the logical resource until explicit owner recovery. No auto reconnect.
  state_.state = state_.owner_activation == 0 ? RuntimeState::idle : RuntimeState::faulted;
  state_.last_code = code;
  complete(code);
  touch();
}
Code Runtime::watchdog(core::safety::DeadlineWatchdog& timer) noexcept {
  const auto status = timer.poll();
  if (status == core::safety::DeadlineWatchdogStatus::ok) return Code::ok;
  return status == core::safety::DeadlineWatchdogStatus::expired ? Code::timeout : Code::clock_fault;
}
bool Runtime::read_now(core::time::MonotonicTime& value) noexcept {
  const auto reading = clock_.read();
  const auto properties = service_timer_.clock_properties();
  if (!core::time::is_valid_clock_reading(reading, properties.domain) ||
      reading.uncertainty > properties.max_uncertainty || reading.value < last_clock_read_) {
    fault(Code::clock_fault); return false;
  }
  last_clock_read_ = reading.value;
  value = reading.value;
  return true;
}
bool Runtime::check_watchdogs() noexcept {
  if (state_.state == RuntimeState::ready) {
    const auto health = watchdog(service_timer_);
    if (health != Code::ok) { fault(health); return false; }
  }
  if (pending_) {
    const auto health = watchdog(operation_timer_);
    if (health == Code::clock_fault) { fault(health); return false; }
    if (health != Code::ok) {
      if (request_.action == RuntimeAction::connect && bus_) fault(health);
      else complete(health);
      return false;
    }
  }
  return true;
}
void Runtime::connect() noexcept {
  if (next_session_ == (std::numeric_limits<std::uint64_t>::max)()) {
    complete(Code::counter_exhausted); return;
  }
  const auto acquired = resources_.acquire(config_.resource, request_.activation);
  // An existing lease cannot be adopted merely by knowing its owner number.
  if (acquired.status != cr::ResourceAcquireStatus::acquired) {
    complete(acquired.status == cr::ResourceAcquireStatus::capacity_exhausted ?
             Code::capacity_exhausted : Code::resource_busy); return;
  }
  lease_ = acquired.lease;
  state_.resources_held = true;
  state_.owner_activation = request_.activation;
  state_.session = ++next_session_;
  bus_.emplace(driver_);
  const auto subscription = bus_->subscribe(
      {config_.response_id, 0x7ff, true, false}, sink_, core::transport::CanSinkExecutionContract{core::time::MonotonicDuration{1000}});
  if (subscription.status != ct::CanSubscriptionStatus::subscribed || !bus_->freeze_configuration()) {
    fault(Code::operation_failed); return;
  }
  const auto opened = bus_->start(config_.channel);
  if (opened != ct::CanStatus::ok) {
    if (opened == ct::CanStatus::busy || opened == ct::CanStatus::already_open) {
      complete(Code::resource_busy); close_bus(); release();
      if (!lease_.valid()) { state_.state = RuntimeState::idle; state_.owner_activation = 0; state_.session = 0; }
      touch(); return;
    }
    fault(Code::transport_error); return;
  }
  state_.state = RuntimeState::ready;
  if (service_timer_.arm(config_.service_timeout) != core::safety::DeadlineWatchdogStatus::ok) {
    fault(Code::clock_fault); return;
  }
  if (!check_watchdogs()) return;
  complete(Code::ok);
}
void Runtime::Sink::on_can_frame(const ct::ReceivedCanFrame& frame) noexcept {
  auto& p = parent_;
  if (!p.pending_ || !p.sent_ || p.request_.action != RuntimeAction::sample ||
      frame.frame.format != ct::CanFrameFormat::classic ||
      frame.frame.type != ct::CanFrameType::data || frame.frame.length != 8 ||
      frame.timestamp.value < p.sent_at_) return;
  std::uint64_t correlation = 0;
  for (std::size_t i = 0; i < 8; ++i)
    correlation = (correlation << 8U) | std::to_integer<std::uint8_t>(frame.frame.payload[i]);
  if (correlation != p.request_.operation) return;
  p.matched_ = true; p.matched_at_ = frame.timestamp.value;
}
void Runtime::sample() noexcept {
  if (!sent_) {
    ct::CanFrame frame{}; frame.identifier = config_.request_id; frame.length = 8;
    auto correlation = request_.operation;
    for (std::size_t i = 8; i > 0; --i) {
      frame.payload[i - 1] = static_cast<std::byte>(correlation & 0xffU);
      correlation >>= 8U;
    }
    if (!read_now(sent_at_)) return;
    const auto status = bus_->send(frame);
    if (status == ct::CanStatus::would_block) return;  // one attempt per executor turn
    if (status != ct::CanStatus::ok) { fault(Code::transport_error); return; }
    sent_ = true;
  }
}
void Runtime::service() noexcept {
  if (servicing_ || state_.state == RuntimeState::stopped) return;
  struct Guard { bool& b; explicit Guard(bool& f) : b(f) { b = true; } ~Guard() { b = false; } } guard{servicing_};
  // Cancellation has its own flag, not a place behind an RX/TX queue.
  if (pending_ && cancellation_.requested(token_)) complete(Code::cancelled);
  if (pending_ && (request_.action == RuntimeAction::disconnect || request_.action == RuntimeAction::recover)) {
    const auto session = state_.session;
    close_bus(); release();
    if (lease_.valid()) { complete(Code::cleanup_failed); return; }
    complete(Code::ok);
    // Completion retains the session being closed, never a new session's identity.
    if (completion_) completion_->session = session;
    state_.state = RuntimeState::idle; state_.owner_activation = 0; state_.session = 0;
    touch(); return;
  }
  core::time::MonotonicTime now{0};
  if (!read_now(now) || !check_watchdogs()) return;
  if (pending_ && request_.action == RuntimeAction::connect) { connect(); return; }
  if (state_.state != RuntimeState::ready) return;
  if (pending_) sample();
  if (state_.state != RuntimeState::ready) return;
  const auto received = bus_->poll(config_.rx_budget);
  increment(state_.received_frames, static_cast<std::uint64_t>(received.frames_received));
  increment(state_.lost_frames, received.rx_dropped);
  if (received.status != ct::CanStatus::ok) { fault(Code::transport_error); return; }
  if (received.rx_dropped != 0) { fault(Code::queue_overflow); return; }
  // A frame does not excuse a missed deadline or an unhealthy executor clock.
  if (!read_now(now) || !check_watchdogs()) return;
  if (matched_ && matched_at_ > now) { fault(Code::clock_fault); return; }
  const auto kick = service_timer_.kick();
  if (kick != core::safety::DeadlineWatchdogStatus::ok) {
    fault(kick == core::safety::DeadlineWatchdogStatus::expired ? Code::timeout : Code::clock_fault);
    return;
  }
  if (matched_) complete(Code::ok, true);
  touch();
}
void Runtime::shutdown() noexcept {
  if (servicing_) return;  // concurrent/reentrant use is outside the contract
  if (state_.state == RuntimeState::stopped) return;
  complete(Code::cancelled); close_bus(); release();
  if (!lease_.valid()) {
    state_.state = RuntimeState::stopped; state_.owner_activation = 0; state_.session = 0;
  }
  touch();
}
}  // namespace ecu::app::runtime
