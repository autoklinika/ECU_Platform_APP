#pragma once

#include "ecu/app/sdk/runtime.hpp"
#include <functional>
#include <string_view>

namespace ecu::app::host {

struct ServiceResult {
  bool success{false};
  std::string text;
};
struct Service {
  std::string id;
  // Trusted, short, synchronous host implementation. Never loaded from a package.
  std::function<ServiceResult(std::string_view)> execute;
  sdk::RuntimeAction runtime_action{sdk::RuntimeAction::none};
};

// Single-caller, offline catalog proof; not a second CORE runtime/resource manager.
// Concurrent calls are unsupported; callback re-entry into mutations returns busy.
// All state returned to callers is a copy. Runtime access uses only the SDK port.
// A supplied runtime outlives Host. Services are immutable after construction.
class Host final {
 public:
  static constexpr std::size_t kMaxModules = 32;
  explicit Host(std::vector<Service> services, sdk::IRuntimePort* runtime = nullptr);
  ~Host();
  Host(const Host&) = delete;
  Host& operator=(const Host&) = delete;
  Host(Host&&) = delete;
  Host& operator=(Host&&) = delete;

  [[nodiscard]] sdk::Result install(const sdk::Manifest& manifest);
  [[nodiscard]] sdk::Result replace(const sdk::Manifest& manifest,
                                    const sdk::Version& expected_version);
  [[nodiscard]] sdk::Result remove(std::string_view id);
  [[nodiscard]] sdk::Result activate(std::string_view id);
  [[nodiscard]] sdk::Result deactivate(std::string_view id, std::uint64_t generation);
  // Host/operator policy APIs, not available to packages. Default-deny.
  [[nodiscard]] sdk::Result allow(std::string_view id, std::string_view operation);
  [[nodiscard]] sdk::Result deny(std::string_view id, std::string_view operation);
  [[nodiscard]] sdk::Result invoke(std::string_view id, std::uint64_t generation,
                                  std::string_view operation, std::string_view input,
                                  std::uint64_t session_id = 0);
  [[nodiscard]] sdk::Result cancel(std::string_view id, std::uint64_t generation,
                                  std::uint64_t operation_id);
  // Called by the trusted runtime executor, never by a GUI sampling timer.
  void service();
  void shutdown_runtime();
  [[nodiscard]] sdk::RuntimeSnapshot runtime_snapshot() const noexcept;
  [[nodiscard]] std::vector<sdk::ModuleSnapshot> catalog() const;

 private:
  [[nodiscard]] sdk::Result compatible(const sdk::Manifest& manifest) const;
  [[nodiscard]] sdk::ModuleSnapshot* find(std::string_view id);
  [[nodiscard]] const Service* service(std::string_view id) const;
  sdk::IRuntimePort* runtime_{nullptr};
  std::vector<Service> services_;
  std::vector<sdk::ModuleSnapshot> modules_;
  bool executing_{false};
  std::uint64_t last_generation_{0};
  std::uint64_t last_operation_{0};
};
}  // namespace ecu::app::host
