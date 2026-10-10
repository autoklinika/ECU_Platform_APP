#include "ecu/app/host/host.hpp"
#include "ecu/app/host/manifest.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace host = ecu::app::host;
namespace sdk = ecu::app::sdk;
using sdk::Code;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (false)
sdk::Manifest manifest(const std::string& id, const std::string& service = "demo.echo") {
  return {1, id, {1, 0, 0}, {1, 0}, {service}, {{"run", service}}};
}
sdk::ModuleSnapshot snapshot(const host::Host& platform, const std::string& id) {
  for (auto m : platform.catalog()) if (m.manifest.id == id) return m;
  std::cerr << "missing snapshot\n"; std::exit(1);
}
std::vector<host::Service> services(int& calls) {
  return {
    {"demo.echo", [&calls](std::string_view text) { ++calls; return host::ServiceResult{true, std::string{text}}; }},
    {"demo.fail", [&calls](std::string_view) { ++calls; return host::ServiceResult{false, "declared failure"}; }},
    {"demo.throw", [](std::string_view) -> host::ServiceResult { throw std::runtime_error("do not leak exception details"); }},
    {"demo.oversize", [](std::string_view) { return host::ServiceResult{true, std::string(513, 'x')}; }},
  };
}

int main() {
  int calls = 0;
  host::Host platform{services(calls)};
  auto alpha = manifest("alpha");
  CHECK(platform.install(alpha).ok());
  CHECK(platform.install(alpha).code == Code::duplicate_id);
  CHECK(snapshot(platform, "alpha").state == sdk::State::disabled);
  CHECK(snapshot(platform, "alpha").allowed_operations.empty());
  CHECK(calls == 0);  // Install never executes package code or services.
  auto incompatible = manifest("new.api"); incompatible.requires_api = {2, 0};
  CHECK(platform.install(incompatible).code == Code::incompatible_api);
  incompatible.requires_api = {1, 2};
  CHECK(platform.install(incompatible).code == Code::incompatible_api);
  auto missing = manifest("missing", "hardware.can");
  CHECK(platform.install(missing).code == Code::missing_capability);
  auto invalid = alpha; invalid.requires_services.clear();
  CHECK(platform.install(invalid).code == Code::invalid_manifest);
  CHECK(platform.catalog().size() == 1);
  CHECK(platform.activate("absent").code == Code::not_found);
  CHECK(platform.invoke("alpha", 1, "run", "value").code == Code::not_active);
  const auto first = platform.activate("alpha");
  CHECK(first.ok() && first.generation != 0);
  CHECK(platform.activate("alpha").code == Code::already_active);
  CHECK(platform.invoke("alpha", first.generation, "run", "value").code == Code::permission_denied);
  CHECK(platform.invoke("alpha", 0, "run", "value").code == Code::stale_activation);
  CHECK(platform.allow("alpha", "not.declared").code == Code::unknown_operation);
  CHECK(platform.allow("alpha", "run").ok());
  CHECK(platform.allow("alpha", "run").ok());
  CHECK(snapshot(platform, "alpha").allowed_operations.size() == 1);
  const auto echo = platform.invoke("alpha", first.generation, "run", "value");
  CHECK(echo.ok() && echo.operation_id == 1 && echo.payload == "value");
  CHECK(echo.generation == first.generation && calls == 1);
  CHECK(platform.invoke("alpha", first.generation, "run", std::string(513, 'x')).code == Code::invalid_input);
  CHECK(platform.invoke("alpha", first.generation, "other", "").code == Code::unknown_operation);
  CHECK(calls == 1);
  CHECK(platform.remove("alpha").code == Code::active_module);
  auto update = alpha; update.version.minor = 1;
  CHECK(platform.replace(update, alpha.version).code == Code::active_module);
  CHECK(snapshot(platform, "alpha").manifest.version == alpha.version);
  CHECK(platform.deactivate("alpha", first.generation + 1).code == Code::stale_activation);
  CHECK(snapshot(platform, "alpha").state == sdk::State::active);

  // Independent packages can use shared capabilities without shared module state.
  const auto beta = manifest("beta", "demo.fail");
  CHECK(platform.install(beta).ok());
  CHECK(platform.allow("beta", "run").ok());
  const auto second = platform.activate("beta");
  CHECK(second.ok() && second.generation > first.generation);
  const auto failed = platform.invoke("beta", second.generation, "run", "");
  CHECK(failed.code == Code::operation_failed && failed.operation_id == 2);
  CHECK(failed.detail == "declared failure");
  CHECK(snapshot(platform, "alpha").last_operation.operation_id == echo.operation_id);
  const auto after_failure = platform.invoke("alpha", first.generation, "run", "still working");
  CHECK(after_failure.ok() && after_failure.operation_id == 3);
  CHECK(snapshot(platform, "beta").last_operation.code == Code::operation_failed);
  CHECK(snapshot(platform, "alpha").completed_operations == 2);

  auto copy = platform.catalog(); copy[0].manifest.id = "tampered"; copy[0].allowed_operations.clear();
  CHECK(snapshot(platform, "alpha").allowed_operations.size() == 1);
  CHECK(platform.deny("alpha", "run").ok());
  CHECK(platform.invoke("alpha", first.generation, "run", "").code == Code::permission_denied);
  CHECK(platform.deactivate("alpha", first.generation).ok());
  CHECK(platform.deactivate("alpha", first.generation).code == Code::not_active);
  CHECK(platform.replace(update, {9, 9, 9}).code == Code::version_conflict);
  CHECK(snapshot(platform, "alpha").manifest.version == alpha.version);
  auto broken_update = update; broken_update.requires_services.push_back("absent.service");
  CHECK(platform.replace(broken_update, alpha.version).code == Code::missing_capability);
  CHECK(snapshot(platform, "alpha").manifest.version == alpha.version);
  CHECK(platform.allow("alpha", "run").ok());
  CHECK(platform.replace(update, alpha.version).ok());
  CHECK(snapshot(platform, "alpha").allowed_operations.empty());
  CHECK(snapshot(platform, "alpha").last_operation.operation_id == 0);
  const auto third = platform.activate("alpha");
  CHECK(third.ok() && third.generation > second.generation);
  CHECK(platform.invoke("alpha", first.generation, "run", "").code == Code::stale_activation);
  CHECK(platform.invoke("alpha", third.generation, "run", "").code == Code::permission_denied);
  CHECK(platform.deactivate("alpha", third.generation).ok());
  CHECK(platform.remove("alpha").ok());
  CHECK(platform.remove("alpha").code == Code::not_found);
  CHECK(platform.install(alpha).ok());
  CHECK(platform.allow("alpha", "run").ok());
  const auto fourth = platform.activate("alpha");
  CHECK(fourth.ok() && fourth.generation > third.generation);
  CHECK(platform.invoke("alpha", third.generation, "run", "").code == Code::stale_activation);
  CHECK(platform.invoke("alpha", fourth.generation, "run", "again").ok());

  for (const auto* cap : {"demo.throw", "demo.oversize"}) {
    CHECK(platform.install(manifest(cap, cap)).ok());
    CHECK(platform.allow(cap, "run").ok());
    const auto active = platform.activate(cap);
    const auto response = platform.invoke(cap, active.generation, "run", "");
    CHECK(response.code == Code::operation_failed);
    CHECK(response.payload.empty());
    CHECK(response.detail.find("do not leak") == std::string::npos);
    CHECK(platform.invoke("alpha", fourth.generation, "run", "healthy").ok());
  }

  host::Host bounded{services(calls)};
  for (std::size_t i = 0; i < host::Host::kMaxModules; ++i)
    CHECK(bounded.install(manifest("module." + std::to_string(i))).ok());
  CHECK(bounded.install(manifest("one.too.many")).code == Code::capacity_exhausted);
  CHECK(bounded.catalog().size() == host::Host::kMaxModules);
  CHECK(bounded.remove("module.0").ok());
  CHECK(bounded.install(manifest("one.too.many")).ok());
  bool duplicate_rejected = false;
  try { host::Host bad{{{"duplicate", [](std::string_view) { return host::ServiceResult{}; }},
                        {"duplicate", [](std::string_view) { return host::ServiceResult{}; }}}}; }
  catch (const std::invalid_argument&) { duplicate_rejected = true; }
  CHECK(duplicate_rejected);

  // Even a buggy trusted service may not mutate the catalog during invocation.
  host::Host* ptr = nullptr;
  host::Host reentry{{{"test.reentry", [&ptr](std::string_view) {
    CHECK(ptr->remove("reentrant").code == Code::busy);
    CHECK(ptr->install(manifest("new.one")).code == Code::busy);
    CHECK(ptr->allow("reentrant", "run").code == Code::busy);
    CHECK(ptr->deny("reentrant", "run").code == Code::busy);
    CHECK(ptr->activate("reentrant").code == Code::busy);
    CHECK(ptr->deactivate("reentrant", 1).code == Code::busy);
    CHECK(ptr->replace(manifest("reentrant", "test.reentry"), {1,0,0}).code == Code::busy);
    CHECK(ptr->invoke("reentrant", 1, "run", "").code == Code::busy);
    CHECK(ptr->catalog().size() == 1);
    return host::ServiceResult{true, "reentry guarded"};
  }}}};
  ptr = &reentry;
  CHECK(reentry.install(manifest("reentrant", "test.reentry")).ok());
  CHECK(reentry.allow("reentrant", "run").ok());
  const auto active = reentry.activate("reentrant");
  CHECK(reentry.invoke("reentrant", active.generation, "run", "").ok());
  CHECK(reentry.deactivate("reentrant", active.generation).ok());
  std::cout << "HOST_LIFECYCLE_POLICY_ISOLATION=PASS\n";
}
