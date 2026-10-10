#pragma once
#include "ecu/app/sdk/contracts.hpp"

namespace ecu::app::sdk {
// Trusted services select these actions; a package cannot submit raw frames.
// Source-level experimental port. All calls belong to the same executor.
enum class RuntimeAction { none, connect, sample, disconnect, recover };
enum class RuntimeState { idle, ready, faulted, stopped };
struct RuntimeRequest {
  RuntimeAction action{RuntimeAction::none};
  std::uint64_t activation{0};
  std::uint64_t operation{0};
  std::uint64_t session{0};
};
struct RuntimeCompletion {
  RuntimeRequest request{};
  Code code{Code::ok};
  std::uint64_t session{0};
  bool observed{false};
  std::int64_t observed_at_ns{0};
};
struct RuntimeSnapshot {
  RuntimeState state{RuntimeState::idle};
  std::uint64_t revision{0};
  std::uint64_t owner_activation{0};
  std::uint64_t session{0};
  std::uint64_t pending_operation{0};
  Code last_code{Code::ok};
  bool resources_held{false};
  std::uint64_t received_frames{0};
  std::uint64_t lost_frames{0};
};
class IRuntimePort {
 public:
  virtual ~IRuntimePort() = default;
  // accepted means queued, never completed. At most one pending request/result.
  [[nodiscard]] virtual Code submit(const RuntimeRequest& request) noexcept = 0;
  [[nodiscard]] virtual Code cancel(std::uint64_t activation,
                                   std::uint64_t operation) noexcept = 0;
  virtual void service() noexcept = 0;
  [[nodiscard]] virtual bool take_completion(RuntimeCompletion& result) noexcept = 0;
  [[nodiscard]] virtual RuntimeSnapshot snapshot() const noexcept = 0;
  // Out-of-band shutdown is never queued behind samples; terminal for this port.
  virtual void shutdown() noexcept = 0;
};
}  // namespace ecu::app::sdk
