#include "ecu/app/host/host.hpp"
#include "ecu/app/host/manifest.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ecu::app::host {
namespace {
using sdk::Code;
using sdk::Result;
Result result(Code code, std::string_view module = {}, std::string_view detail = {}) {
  return {code, std::string{module}, std::string{detail}, 0, 0, {}};
}
const sdk::Operation* operation(const sdk::ModuleSnapshot& module, const std::string_view id) {
  const auto it = std::find_if(module.manifest.operations.begin(), module.manifest.operations.end(),
                              [&](const sdk::Operation& op) { return op.id == id; });
  return it == module.manifest.operations.end() ? nullptr : &*it;
}
}  // namespace

Host::Host(std::vector<Service> services) : services_(std::move(services)) {
  if (services_.size() > sdk::kMaxItems) throw std::invalid_argument("too many services");
  for (std::size_t i = 0; i < services_.size(); ++i) {
    if (!valid_name(services_[i].id) || !services_[i].execute)
      throw std::invalid_argument("invalid service");
    for (std::size_t j = 0; j < i; ++j) {
      if (services_[i].id == services_[j].id) throw std::invalid_argument("duplicate service");
    }
  }
  modules_.reserve(kMaxModules);
}
const Service* Host::service(const std::string_view id) const {
  const auto it = std::find_if(services_.begin(), services_.end(), [&](const Service& s) { return s.id == id; });
  return it == services_.end() ? nullptr : &*it;
}
sdk::ModuleSnapshot* Host::find(const std::string_view id) {
  const auto it = std::find_if(modules_.begin(), modules_.end(), [&](const sdk::ModuleSnapshot& m) { return m.manifest.id == id; });
  return it == modules_.end() ? nullptr : &*it;
}
Result Host::compatible(const sdk::Manifest& m) const {
  auto checked = validate_manifest(m);
  if (!checked.ok()) return checked;
  if (m.requires_api.major != sdk::kHostApi.major || m.requires_api.minor > sdk::kHostApi.minor)
    return result(Code::incompatible_api, m.id, "unsupported API requirement");
  for (const auto& cap : m.requires_services) {
    if (service(cap) == nullptr) return result(Code::missing_capability, m.id, cap);
  }
  return result(Code::ok, m.id);
}
Result Host::install(const sdk::Manifest& m) {
  if (executing_) return result(Code::busy);
  auto checked = compatible(m);
  if (!checked.ok()) return checked;
  if (find(m.id) != nullptr) return result(Code::duplicate_id, m.id);
  if (modules_.size() == kMaxModules) return result(Code::capacity_exhausted, m.id);
  sdk::ModuleSnapshot next;
  next.manifest = m;
  modules_.push_back(std::move(next));
  return result(Code::ok, m.id, "installed disabled");
}
Result Host::replace(const sdk::Manifest& m, const sdk::Version& expected_version) {
  if (executing_) return result(Code::busy);
  auto checked = compatible(m);
  if (!checked.ok()) return checked;
  auto* current = find(m.id);
  if (current == nullptr) return result(Code::not_found, m.id);
  if (current->state == sdk::State::active) return result(Code::active_module, m.id);
  if (current->manifest.version != expected_version) return result(Code::version_conflict, m.id);
  sdk::ModuleSnapshot next;
  next.manifest = m;  // Prepare complete replacement before publishing; resets grants/results.
  *current = std::move(next);
  return result(Code::ok, m.id, "replaced disabled; grants cleared");
}
Result Host::remove(const std::string_view id) {
  if (executing_) return result(Code::busy);
  auto* current = find(id);
  if (current == nullptr) return result(Code::not_found, id);
  if (current->state == sdk::State::active) return result(Code::active_module, id);
  auto response = result(Code::ok, id, "removed");
  modules_.erase(std::remove_if(modules_.begin(), modules_.end(), [&](const sdk::ModuleSnapshot& m) { return m.manifest.id == id; }), modules_.end());
  return response;
}
Result Host::activate(const std::string_view id) {
  if (executing_) return result(Code::busy);
  auto* current = find(id);
  if (current == nullptr) return result(Code::not_found, id);
  if (current->state == sdk::State::active) return result(Code::already_active, id);
  if (last_generation_ == (std::numeric_limits<std::uint64_t>::max)()) return result(Code::counter_exhausted, id);
  current->state = sdk::State::active;
  current->generation = ++last_generation_;
  current->completed_operations = 0;
  current->last_operation = {};
  auto response = result(Code::ok, id, "activated");
  response.generation = current->generation;
  return response;
}
Result Host::deactivate(const std::string_view id, const std::uint64_t generation) {
  if (executing_) return result(Code::busy);
  auto* current = find(id);
  if (current == nullptr) return result(Code::not_found, id);
  if (current->state != sdk::State::active) return result(Code::not_active, id);
  if (generation == 0 || current->generation != generation) return result(Code::stale_activation, id);
  current->state = sdk::State::disabled;
  auto response = result(Code::ok, id, "disabled");
  response.generation = generation;
  return response;
}
Result Host::allow(const std::string_view id, const std::string_view op) {
  if (executing_) return result(Code::busy);
  auto* current = find(id);
  if (current == nullptr) return result(Code::not_found, id);
  if (operation(*current, op) == nullptr) return result(Code::unknown_operation, id);
  auto& allowed = current->allowed_operations;
  if (std::find(allowed.begin(), allowed.end(), op) == allowed.end()) allowed.emplace_back(op);
  return result(Code::ok, id, "allowed by operator");
}
Result Host::deny(const std::string_view id, const std::string_view op) {
  if (executing_) return result(Code::busy);
  auto* current = find(id);
  if (current == nullptr) return result(Code::not_found, id);
  if (operation(*current, op) == nullptr) return result(Code::unknown_operation, id);
  auto& allowed = current->allowed_operations;
  allowed.erase(std::remove(allowed.begin(), allowed.end(), op), allowed.end());
  return result(Code::ok, id, "denied by operator");
}
Result Host::invoke(const std::string_view id, const std::uint64_t generation,
                    const std::string_view op, const std::string_view input) {
  if (executing_) return result(Code::busy);
  auto* current = find(id);
  if (current == nullptr) return result(Code::not_found, id);
  if (current->state != sdk::State::active) return result(Code::not_active, id);
  if (generation == 0 || current->generation != generation) return result(Code::stale_activation, id);
  const auto* spec = operation(*current, op);
  if (spec == nullptr) return result(Code::unknown_operation, id);
  if (std::find(current->allowed_operations.begin(), current->allowed_operations.end(), op) == current->allowed_operations.end())
    return result(Code::permission_denied, id);
  if (!valid_text(input)) return result(Code::invalid_input, id);
  const auto* implementation = service(spec->service);
  if (implementation == nullptr) return result(Code::missing_capability, id);
  if (last_operation_ == (std::numeric_limits<std::uint64_t>::max)()) return result(Code::counter_exhausted, id);
  auto response = result(Code::ok, id);
  response.operation_id = ++last_operation_;
  response.generation = generation;
  struct ExecutionGuard {
    bool& flag;
    explicit ExecutionGuard(bool& value) : flag(value) { flag = true; }
    ~ExecutionGuard() { flag = false; }
  } guard{executing_};
  try {
    auto performed = implementation->execute(input);
    if (!valid_text(performed.text)) {
      response.code = Code::operation_failed;
      response.detail = "service result violates contract";
    } else if (!performed.success) {
      response.code = Code::operation_failed;
      response.detail = std::move(performed.text);
    } else {
      response.payload = std::move(performed.text);
    }
  } catch (...) {
    // C++ exceptions only. No claim of native-crash isolation or watchdog behavior.
    response.code = Code::operation_failed;
    response.detail = "trusted service exception";
  }
  ++current->completed_operations;
  current->last_operation = response;
  return response;
}
std::vector<sdk::ModuleSnapshot> Host::catalog() const { return modules_; }
}  // namespace ecu::app::host
