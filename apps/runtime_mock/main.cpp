#include "ecu/app/host/host.hpp"
#include "ecu/app/host/manifest.hpp"
#include "ecu/app/runtime/runtime.hpp"
#include "ecu/app/testing/mock_can.hpp"
#include "package_reader.hpp"
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <string>

namespace sdk=ecu::app::sdk;
namespace host=ecu::app::host;
namespace mock=ecu::app::testing;
using sdk::Code;
void require(bool ok) { if(!ok) throw std::runtime_error("mock proof did not match expected contract"); }
int main(int argc,char** argv) {
  if(argc!=2) { std::cerr << "Usage: ecu_runtime_mock path-to-mock-package (virtual time; NO HARDWARE)\n"; return 2; }
  try {
    auto parsed=read_package(argv[1]); require(parsed.result.ok());
    mock::ManualClock clock; mock::MockChannel channel; mock::MockCan driver{channel,clock};
    ecu::core::v2::runtime::ResourceManager resources;
    ecu::app::runtime::Runtime runtime{driver,clock,resources};
    host::Host platform{{{"mock.connect",{},sdk::RuntimeAction::connect},
                         {"mock.sample",{},sdk::RuntimeAction::sample},
                         {"mock.disconnect",{},sdk::RuntimeAction::disconnect},
                         {"mock.recover",{},sdk::RuntimeAction::recover}},&runtime};
    const auto& id=parsed.manifest.id;
    require(platform.install(parsed.manifest).ok());
    const auto active=platform.activate(id); require(active.ok());
    for(const auto& op:parsed.manifest.operations) require(platform.allow(id,op.id).ok());
    auto operation=[&](const std::string& service) {
      for(const auto& op:parsed.manifest.operations) if(op.service==service) return op.id;
      throw std::runtime_error("missing proof operation");
    };
    auto show=[&](const char* event,const sdk::Result& result) {
      std::cout << "{\"event\":" << std::quoted(event) << ",\"status\":" << std::quoted(host::code_name(result.code))
                << ",\"operation_id\":" << result.operation_id << ",\"session_id\":" << result.session_id
                << ",\"resource_count\":" << resources.active_count() << ",\"open_count\":" << driver.opens
                << ",\"send_count\":" << driver.sends << "}\n";
    };
    auto last=[&]() { return platform.catalog().front().last_operation; };
    auto issue=[&](const char* service,std::uint64_t session) {
      auto result=platform.invoke(id,active.generation,operation(service),"",session);
      require(result.code==Code::accepted); show("accepted",result); return result;
    };
    issue("mock.connect",0); require(driver.opens==0); platform.service();
    auto connection=last(); require(connection.ok()); show("connected",connection);
    const auto session=connection.session_id;
    for(int i=0;i<3;++i) {
      issue("mock.sample",session); clock.advance(std::chrono::milliseconds{1}); platform.service();
      require(last().ok()); show("sample",last());
    }
    require(driver.opens==1);
    driver.respond=false;
    issue("mock.sample",session); platform.service();
    clock.advance(std::chrono::milliseconds{51}); platform.service();
    require(last().code==Code::timeout); show("timeout",last());
    driver.respond=true; driver.next_send=ecu::core::v2::transport::CanStatus::bus_off;
    issue("mock.sample",session); platform.service();
    require(last().code==Code::transport_error && !driver.is_open()); show("bus_off",last());
    issue("mock.recover",session); platform.service();
    require(last().ok() && resources.active_count()==0); show("recovered",last());
    issue("mock.connect",0); platform.service();
    connection=last(); require(connection.ok() && connection.session_id>session); show("reconnected",connection);
    issue("mock.sample",connection.session_id);
    const auto sent=driver.sends; platform.shutdown_runtime();
    require(last().code==Code::cancelled && driver.sends==sent);
    require(resources.active_count()==0 && !driver.is_open() && channel.owner.load()==nullptr);
    show("shutdown",last()); return 0;
  } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
