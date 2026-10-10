#pragma once
#include "ecu/core_v2/transport/i_can_driver.hpp"
#include <array>
#include <atomic>
#include <limits>

namespace ecu::app::testing {
namespace core = ecu::core::v2;
namespace ct = core::transport;
namespace tm = core::time;
// Explicit virtual clock: properties describe the simulation, not an OS clock.
class ManualClock final : public tm::IMonotonicClock {
 public:
  tm::MonotonicTime now{1000000000};
  mutable std::uint64_t reads{0};
  std::uint64_t unavailable_at{0};
  tm::MonotonicClockStatus health{tm::MonotonicClockStatus::ok};
  [[nodiscard]] tm::MonotonicClockProperties properties() const noexcept override {
    return {{1}, tm::MonotonicDuration{1}, tm::MonotonicDuration{1}, tm::MonotonicDuration{0}, true};
  }
  [[nodiscard]] tm::MonotonicClockReading read() const noexcept override {
    ++reads;
    return {reads == unavailable_at ? tm::MonotonicClockStatus::unavailable : health, {1}, now, tm::MonotonicDuration{0}};
  }
  void advance(const tm::MonotonicDuration amount) noexcept { now += amount; }
};
// Same channel object MUST be shared by every driver wrapper for that channel.
class MockChannel final : public ct::ICanChannelArbiter {
 public:
  const ct::CanPhysicalChannelId id{1};
  std::atomic<const void*> owner{nullptr};
  [[nodiscard]] bool try_acquire(const ct::CanPhysicalChannelId channel,
                                 const void* token) noexcept override {
    if (channel != id || token == nullptr) return false;
    const void* expected = nullptr;
    return owner.compare_exchange_strong(expected, token) || expected == token;
  }
  void release(const ct::CanPhysicalChannelId channel, const void* token) noexcept override {
    if (channel == id) { const void* expected = token; static_cast<void>(owner.compare_exchange_strong(expected, nullptr)); }
  }
};
// Contract fixture, NOT an electrical CAN simulation or real device driver.
// Test hooks are private to composition/tests and never exposed by SDK or Host.
class MockCan final : public ct::ICanDriver {
 public:
  static constexpr std::size_t kCapacity = 16;
  MockCan(MockChannel& channel, ManualClock& clock) : channel_(channel), clock_(clock) {}
  std::uint64_t opens{0}, closes{0}, sends{0}, send_attempts{0}, receive_attempts{0};
  bool respond{true};
  bool block_tx{false};
  ct::CanStatus open_result{ct::CanStatus::ok};
  ct::CanStatus next_send{ct::CanStatus::ok};
  ct::CanStatus next_receive{ct::CanStatus::ok};
  tm::MonotonicDuration receive_cost{0};
  ct::CanFrame last_sent{};
  [[nodiscard]] ct::CanPhysicalChannelId physical_channel_id() const noexcept override { return channel_.id; }
  [[nodiscard]] ct::ICanChannelArbiter& channel_arbiter() noexcept override { return channel_; }
  [[nodiscard]] ct::CanDriverExecutionContract execution_contract() const noexcept override {
    return {tm::MonotonicDuration{1000}, tm::MonotonicDuration{1000},
            tm::MonotonicDuration{1000}, tm::MonotonicDuration{1000},
            tm::MonotonicDuration{1000}, tm::MonotonicDuration{1000}, tm::MonotonicDuration{1}};
  }
  [[nodiscard]] ct::CanCapabilities capabilities() const noexcept override { return {true, false, false, true, 8}; }
  [[nodiscard]] ct::CanStatus open(const ct::CanChannelConfig& config) noexcept override {
    ++opens;
    if (open_) return ct::CanStatus::already_open;
    head_ = 0; size_ = 0; dropped_ = 0; quarantined_ = false;
    if (open_result != ct::CanStatus::ok) return open_result;
    if (!ct::is_valid_can_channel_config(config) || channel_.owner.load() == nullptr) return ct::CanStatus::invalid_state;
    open_ = true; return ct::CanStatus::ok;
  }
  void close() noexcept override {
    if (open_) ++closes;
    open_ = false; size_ = 0; head_ = 0; dropped_ = 0; quarantined_ = false;
  }
  [[nodiscard]] bool is_open() const noexcept override { return open_; }
  [[nodiscard]] ct::CanStatus try_send(const ct::CanFrame& frame) noexcept override {
    ++send_attempts;
    if (!open_ || quarantined_) return ct::CanStatus::not_open;
    const auto injected = next_send; next_send = ct::CanStatus::ok;
    if (injected != ct::CanStatus::ok) {
      if (injected != ct::CanStatus::would_block) { size_ = 0; quarantined_ = true; }
      return injected;
    }
    if (block_tx) return ct::CanStatus::would_block;
    ++sends; last_sent = frame;
    if (respond) {
      auto reply = frame; ++reply.identifier;  // different CAN ID: synthetic peer reply, not local echo
      static_cast<void>(inject({reply, clock_.read()}));
    }
    return ct::CanStatus::ok;
  }
  [[nodiscard]] ct::CanReceiveResult try_receive() noexcept override {
    ++receive_attempts;
    clock_.advance(receive_cost);  // fault injection can intentionally violate declared WCET
    if (!open_ || quarantined_) return {ct::CanStatus::not_open, {}, 0};
    const auto injected = next_receive; next_receive = ct::CanStatus::ok;
    const auto dropped = dropped_; dropped_ = 0;
    if (injected != ct::CanStatus::ok) {
      if (injected != ct::CanStatus::would_block) { size_ = 0; quarantined_ = true; }
      return {injected, {}, dropped};
    }
    if (size_ == 0) return {ct::CanStatus::would_block, {}, dropped};
    const auto result = queue_[head_]; head_ = (head_ + 1) % kCapacity; --size_;
    return {ct::CanStatus::ok, result, dropped};
  }
  [[nodiscard]] bool inject(const ct::ReceivedCanFrame& frame) noexcept {
    if (!open_ || quarantined_) return false;
    if (size_ == kCapacity) {
      if (dropped_ != (std::numeric_limits<std::uint32_t>::max)()) ++dropped_;
      return false;
    }
    queue_[(head_ + size_) % kCapacity] = frame; ++size_; return true;
  }
  [[nodiscard]] bool reply(const std::uint64_t operation) noexcept {
    ct::CanFrame frame{}; frame.identifier = 0x601; frame.length = 8;
    auto value = operation;
    for (std::size_t i = 8; i > 0; --i) { frame.payload[i-1] = static_cast<std::byte>(value & 0xffU); value >>= 8U; }
    return inject({frame, clock_.read()});
  }
  [[nodiscard]] std::size_t queued() const noexcept { return size_; }
 private:
  MockChannel& channel_;
  ManualClock& clock_;
  std::array<ct::ReceivedCanFrame, kCapacity> queue_{};
  std::size_t head_{0}, size_{0};
  std::uint32_t dropped_{0};
  bool open_{false}, quarantined_{false};
};
}  // namespace ecu::app::testing
