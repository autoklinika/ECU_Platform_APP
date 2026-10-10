#pragma once
#include "ecu/app/sdk/runtime.hpp"
#include "ecu/core_v2/runtime/cancellation.hpp"
#include "ecu/core_v2/runtime/resource_manager.hpp"
#include "ecu/core_v2/safety/deadline_watchdog.hpp"
#include "ecu/core_v2/transport/can_bus_runtime.hpp"
#include <optional>

namespace ecu::app::runtime {
namespace core = ecu::core::v2;
// Trusted, immutable composition. The exchange below is a MOCK protocol only.
// This is not a hardware-capable UDS/CAN endpoint or an OEM module API.
struct Config {
  core::runtime::ResourceKey resource{core::runtime::ResourceClass::can_channel, 1};
  core::transport::CanChannelConfig channel{500000, false, 0,
      core::transport::CanMode::normal, {1}};
  core::time::MonotonicDuration operation_timeout{50000000};
  core::time::MonotonicDuration service_timeout{100000000};
  std::uint32_t request_id{0x600};
  std::uint32_t response_id{0x601};
  std::size_t rx_budget{8};
};
// One externally serialized executor calls every method (including snapshots).
// No threads, OS calls, sleeps or blocking I/O. Drivers/clocks/resources outlive it.
// A future OS executor must supply regular service calls; this proof has virtual time.
class Runtime final : public sdk::IRuntimePort {
 public:
  Runtime(core::transport::ICanDriver& driver,
          const core::time::IMonotonicClock& clock,
          core::runtime::ResourceManager& resources, Config config = {});
  ~Runtime() override;
  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;
  Runtime(Runtime&&) = delete;
  Runtime& operator=(Runtime&&) = delete;
  [[nodiscard]] sdk::Code submit(const sdk::RuntimeRequest& request) noexcept override;
  [[nodiscard]] sdk::Code cancel(std::uint64_t activation,
                               std::uint64_t operation) noexcept override;
  void service() noexcept override;
  [[nodiscard]] bool take_completion(sdk::RuntimeCompletion& result) noexcept override;
  [[nodiscard]] sdk::RuntimeSnapshot snapshot() const noexcept override;
  void shutdown() noexcept override;
 private:
  struct Sink final : core::transport::ICanFrameSink {
    explicit Sink(Runtime& parent) : parent_(parent) {}
    void on_can_frame(const core::transport::ReceivedCanFrame& frame) noexcept override;
    Runtime& parent_;
  };
  void connect() noexcept;
  void sample() noexcept;
  void complete(sdk::Code code, bool observed = false) noexcept;
  void fault(sdk::Code code) noexcept;
  void close_bus() noexcept;
  void release() noexcept;
  void touch() noexcept;
  [[nodiscard]] sdk::Code watchdog(core::safety::DeadlineWatchdog& timer) noexcept;
  [[nodiscard]] bool check_watchdogs() noexcept;
  [[nodiscard]] bool read_now(core::time::MonotonicTime& value) noexcept;
  core::transport::ICanDriver& driver_;
  const core::time::IMonotonicClock& clock_;
  core::runtime::ResourceManager& resources_;
  Config config_;
  core::runtime::ResourceLease lease_{};
  core::runtime::CancellationSource cancellation_;
  core::runtime::CancellationToken token_{};
  core::safety::DeadlineWatchdog operation_timer_;
  core::safety::DeadlineWatchdog service_timer_;
  // Sink lifetime includes optional bus destruction, even across stop/recovery.
  Sink sink_;
  std::optional<core::transport::CanBusRuntime> bus_;
  sdk::RuntimeSnapshot state_{};
  sdk::RuntimeRequest request_{};
  std::optional<sdk::RuntimeCompletion> completion_;
  std::uint64_t next_session_{0};
  bool pending_{false};
  bool sent_{false};
  bool matched_{false};
  bool servicing_{false};
  core::time::MonotonicTime sent_at_{0};
  core::time::MonotonicTime matched_at_{0};
  core::time::MonotonicTime last_clock_read_{0};
};
}  // namespace ecu::app::runtime
