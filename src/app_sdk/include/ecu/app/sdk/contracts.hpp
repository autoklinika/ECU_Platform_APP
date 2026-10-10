#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ecu::app::sdk {

// Source-level, experimental API. Not a wire format or a native plugin ABI.
struct ApiVersion {
  std::uint16_t major{1};
  std::uint16_t minor{0};
};
inline constexpr ApiVersion kHostApi{1, 1};
inline constexpr std::size_t kMaxManifestBytes = 8192;
inline constexpr std::size_t kMaxLineBytes = 512;
inline constexpr std::size_t kMaxItems = 16;
inline constexpr std::size_t kMaxTextBytes = 512;

struct Version {
  std::uint16_t major{0};
  std::uint16_t minor{0};
  std::uint16_t patch{0};
};
inline bool operator==(const Version& a, const Version& b) noexcept {
  return a.major == b.major && a.minor == b.minor && a.patch == b.patch;
}
inline bool operator!=(const Version& a, const Version& b) noexcept { return !(a == b); }

struct Operation {
  std::string id;
  std::string service;
};
struct Manifest {
  std::uint16_t schema{1};
  std::string id;
  Version version;
  ApiVersion requires_api;
  std::vector<std::string> requires_services;
  std::vector<Operation> operations;
};

enum class Code {
  ok,
  invalid_manifest,
  incompatible_api,
  missing_capability,
  duplicate_id,
  capacity_exhausted,
  not_found,
  already_active,
  not_active,
  stale_activation,
  active_module,
  busy,
  version_conflict,
  unknown_operation,
  permission_denied,
  invalid_input,
  operation_failed,
  counter_exhausted,
  io_error,
  invalid_command,
  accepted,
  stale_session,
  resource_busy,
  timeout,
  cancelled,
  transport_error,
  clock_fault,
  queue_overflow,
  cleanup_failed,
  invalid_state,
};

enum class State { disabled, active };

struct Result {
  Code code{Code::ok};
  std::string module;
  std::string detail;
  std::uint64_t generation{0};
  std::uint64_t operation_id{0};
  std::string payload;
  std::uint64_t session_id{0};
  std::int64_t observed_at_ns{0};
  [[nodiscard]] bool ok() const noexcept { return code == Code::ok; }
};
struct ModuleSnapshot {
  Manifest manifest;
  State state{State::disabled};
  std::uint64_t generation{0};
  std::vector<std::string> allowed_operations;
  std::uint64_t completed_operations{0};
  Result last_operation;
  Result pending_operation;
};

}  // namespace ecu::app::sdk
