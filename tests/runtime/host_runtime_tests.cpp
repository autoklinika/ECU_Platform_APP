#include "ecu/app/host/host.hpp"
#include "ecu/app/runtime/runtime.hpp"
#include "ecu/app/testing/mock_can.hpp"
#include <cstdlib>
#include <iostream>

namespace sdk=ecu::app::sdk;
namespace host=ecu::app::host;
namespace rt=ecu::app::runtime;
namespace mock=ecu::app::testing;
using sdk::Code;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while(false)
sdk::ModuleSnapshot view(const host::Host& h, const std::string& id) {
  for(auto m:h.catalog()) if(m.manifest.id==id) return m;
  std::exit(2);
}
sdk::Manifest module(const std::string& id) {
  return {1,id,{1,0,0},{1,1},{"mock.connect","mock.sample","mock.disconnect","mock.recover"},
         {{"connect","mock.connect"},{"sample","mock.sample"},{"disconnect","mock.disconnect"},{"recover","mock.recover"}}};
}
std::vector<host::Service> services() {
  return {{"mock.connect",{},sdk::RuntimeAction::connect},
          {"mock.sample",{},sdk::RuntimeAction::sample},
          {"mock.disconnect",{},sdk::RuntimeAction::disconnect},
          {"mock.recover",{},sdk::RuntimeAction::recover},
          {"demo.echo",[](std::string_view in){return host::ServiceResult{true,std::string{in}};}}};
}
int main() {
  mock::ManualClock clock; mock::MockChannel channel; mock::MockCan driver{channel,clock};
  ecu::core::v2::runtime::ResourceManager resources;
  rt::Runtime runtime{driver,clock,resources};
  {
    host::Host platform{services(),&runtime};
    CHECK(platform.install(module("first")).ok()); CHECK(platform.install(module("second")).ok());
    CHECK(platform.install({1,"report",{1,0,0},{1,0},{"demo.echo"},{{"run","demo.echo"}}}).ok());
    const auto a=platform.activate("first").generation;
    const auto b=platform.activate("second").generation;
    const auto report=platform.activate("report").generation;
    CHECK(platform.invoke("first",a,"connect","").code==Code::permission_denied);
    CHECK(driver.opens==0);
    for(const auto* id:{"first","second"}) for(const auto* op:{"connect","sample","disconnect","recover"}) CHECK(platform.allow(id,op).ok());
    CHECK(platform.allow("report","run").ok());
    auto accepted=platform.invoke("first",a,"connect","");
    CHECK(accepted.code==Code::accepted && !accepted.ok() && accepted.operation_id!=0);
    CHECK(driver.opens==0 && view(platform,"first").completed_operations==0);
    CHECK(platform.deactivate("first",a).code==Code::busy);
    platform.service();
    auto connected=view(platform,"first").last_operation;
    CHECK(connected.ok() && connected.operation_id==accepted.operation_id);
    const auto session=connected.session_id;
    CHECK(session!=0 && resources.active_count()==1 && driver.opens==1);
    CHECK(platform.deactivate("first",a).code==Code::active_module);
    CHECK(platform.invoke("second",b,"connect","").code==Code::resource_busy);
    CHECK(platform.invoke("second",b,"sample","",session).code==Code::permission_denied);
    CHECK(platform.invoke("first",a,"sample","",session+1).code==Code::stale_session);
    CHECK(platform.invoke("first",a,"sample","raw CAN here",session).code==Code::invalid_input);
    driver.respond=false;
    accepted=platform.invoke("first",a,"sample","",session);
    CHECK(accepted.code==Code::accepted);
    const auto before=driver.sends;
    CHECK(platform.invoke("report",report,"run","unrelated app works").ok());
    CHECK(platform.deny("first","sample").ok()); // revokes pending work before any TX
    platform.service();
    CHECK(view(platform,"first").last_operation.code==Code::cancelled && driver.sends==before);
    CHECK(view(platform,"report").last_operation.payload=="unrelated app works");
    CHECK(platform.allow("first","sample").ok());
    accepted=platform.invoke("first",a,"sample","",session); platform.service();
    CHECK(platform.cancel("second",b,accepted.operation_id).code==Code::not_found);
    CHECK(platform.cancel("first",a+1,accepted.operation_id).code==Code::stale_activation);
    CHECK(platform.cancel("first",a,accepted.operation_id+1).code==Code::stale_activation);
    CHECK(platform.cancel("first",a,accepted.operation_id).code==Code::accepted);
    platform.service(); CHECK(view(platform,"first").last_operation.code==Code::cancelled);
    const auto count=view(platform,"first").completed_operations;
    platform.service(); CHECK(view(platform,"first").completed_operations==count);
    driver.respond=true;
    CHECK(platform.invoke("first",a,"sample","",session).code==Code::accepted);
    platform.service(); CHECK(view(platform,"first").last_operation.ok());
    CHECK(view(platform,"first").last_operation.observed_at_ns>0 && driver.opens==1);
    driver.next_receive=ecu::core::v2::transport::CanStatus::bus_off;
    CHECK(platform.invoke("first",a,"sample","",session).code==Code::accepted);
    platform.service(); CHECK(view(platform,"first").last_operation.code==Code::transport_error);
    CHECK(platform.runtime_snapshot().state==sdk::RuntimeState::faulted);
    CHECK(platform.invoke("report",report,"run","still independent").ok());
    CHECK(platform.invoke("first",a,"recover","",session).code==Code::accepted);
    platform.service(); CHECK(resources.active_count()==0);
    CHECK(platform.deactivate("first",a).ok());
    const auto new_a=platform.activate("first").generation;
    CHECK(platform.invoke("first",a,"connect","").code==Code::stale_activation);
    CHECK(platform.invoke("first",new_a,"connect","").code==Code::accepted); platform.service();
    const auto new_session=view(platform,"first").last_operation.session_id;
    CHECK(new_session>session);
    CHECK(platform.invoke("first",new_a,"sample","",session).code==Code::stale_session);
    CHECK(platform.invoke("first",new_a,"disconnect","",new_session).code==Code::accepted); platform.service();
    CHECK(!driver.is_open() && resources.active_count()==0 && channel.owner.load()==nullptr);
    CHECK(platform.deactivate("first",new_a).ok()); CHECK(platform.remove("first").ok());
    CHECK(platform.invoke("second",b,"connect","").code==Code::accepted); platform.service();
    CHECK(driver.is_open());
    const auto pending=platform.invoke("second",b,"sample","",view(platform,"second").last_operation.session_id);
    CHECK(pending.code==Code::accepted);
    platform.shutdown_runtime();
    CHECK(view(platform,"second").last_operation.code==Code::cancelled);
    CHECK(!driver.is_open() && resources.active_count()==0);
  }
  CHECK(channel.owner.load()==nullptr);
  std::cout << "HOST_CORE_RUNTIME_INTEGRATION=PASS\n";
}
