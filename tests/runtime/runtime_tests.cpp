#include "ecu/app/runtime/runtime.hpp"
#include "ecu/app/testing/mock_can.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <stdexcept>

namespace rt = ecu::app::runtime;
namespace sdk = ecu::app::sdk;
namespace mock = ecu::app::testing;
namespace core = ecu::core::v2;
using sdk::Code;
using sdk::RuntimeAction;
using sdk::RuntimeState;
using namespace std::chrono_literals;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (false)
struct Fixture {
  mock::ManualClock clock;
  mock::MockChannel channel;
  mock::MockCan driver{channel, clock};
  core::runtime::ResourceManager resources;
  rt::Runtime runtime{driver, clock, resources};
  std::uint64_t operation{0};
  sdk::RuntimeCompletion execute(RuntimeAction action, std::uint64_t owner = 10, std::uint64_t session = 0) {
    CHECK(runtime.submit({action,owner,++operation,session}) == Code::accepted);
    runtime.service();
    sdk::RuntimeCompletion out;
    CHECK(runtime.take_completion(out));
    return out;
  }
};
void lifecycle() {
  Fixture f;
  CHECK(core::transport::is_valid_can_driver_execution_contract(f.driver.execution_contract()));
  CHECK(f.runtime.snapshot().state == RuntimeState::idle);
  CHECK(f.runtime.submit({RuntimeAction::connect,10,++f.operation,0}) == Code::accepted);
  CHECK(f.driver.opens == 0 && f.resources.active_count() == 0);
  f.runtime.service(); sdk::RuntimeCompletion c;
  CHECK(f.runtime.take_completion(c) && c.code == Code::ok && c.session != 0);
  const auto session = c.session;
  CHECK(f.resources.active_count() == 1 && f.channel.owner.load() != nullptr);
  CHECK(f.driver.is_open() && f.driver.opens == 1);
  auto saved = f.runtime.snapshot(); saved.session = 999;
  CHECK(f.runtime.snapshot().session == session);
  for (int i=0;i<4;++i) {
    f.clock.advance(1ms);
    const auto sample = f.execute(RuntimeAction::sample,10,session);
    CHECK(sample.code == Code::ok && sample.observed && sample.session == session);
    CHECK(sample.observed_at_ns == f.clock.now.count());
  }
  CHECK(f.driver.opens == 1 && f.driver.sends == 4 && f.driver.closes == 0);
  auto done=f.execute(RuntimeAction::disconnect,10,session);
  CHECK(done.code == Code::ok && done.session == session);
  CHECK(f.resources.active_count() == 0 && f.channel.owner.load() == nullptr);
  CHECK(!f.driver.is_open() && f.driver.queued() == 0);
  CHECK(f.runtime.snapshot().state == RuntimeState::idle);
  auto again = f.execute(RuntimeAction::connect);
  CHECK(again.code == Code::ok && again.session > session);
  CHECK(f.runtime.submit({RuntimeAction::sample,10,++f.operation,session}) == Code::stale_session);
  CHECK(f.runtime.submit({RuntimeAction::sample,11,++f.operation,again.session}) == Code::permission_denied);
  f.runtime.shutdown(); f.runtime.shutdown(); f.runtime.service();
  CHECK(f.resources.active_count() == 0 && f.channel.owner.load() == nullptr);
  CHECK(f.runtime.submit({RuntimeAction::connect,10,++f.operation,0}) == Code::invalid_state);
  CHECK(saved.revision > 0);
}
void ownership() {
  Fixture f;
  auto c=f.execute(RuntimeAction::connect);
  mock::MockCan second{f.channel,f.clock};
  rt::Runtime peer{second,f.clock,f.resources};
  CHECK(peer.submit({RuntimeAction::connect,20,1,0}) == Code::accepted);
  peer.service(); sdk::RuntimeCompletion out;
  CHECK(peer.take_completion(out) && out.code == Code::resource_busy);
  CHECK(second.opens == 0 && f.driver.closes == 0);
  // Even another host/resource manager cannot bypass the driver's shared arbiter.
  core::runtime::ResourceManager different;
  rt::Config config; config.resource.instance=7;
  rt::Runtime alias{second,f.clock,different,config};
  CHECK(alias.submit({RuntimeAction::connect,30,1,0}) == Code::accepted);
  alias.service(); CHECK(alias.take_completion(out) && out.code == Code::resource_busy);
  CHECK(different.active_count()==0 && second.opens==0);
  CHECK(f.execute(RuntimeAction::sample,10,c.session).code==Code::ok);
  const auto existing=f.resources.acquire({core::runtime::ResourceClass::can_channel,1},10);
  CHECK(existing.status == core::runtime::ResourceAcquireStatus::already_owned);
  CHECK(f.execute(RuntimeAction::disconnect,10,c.session).code==Code::ok);
  CHECK(peer.submit({RuntimeAction::connect,20,2,0})==Code::accepted);
  peer.service(); CHECK(peer.take_completion(out) && out.code==Code::ok);
  CHECK(f.resources.release(existing.lease)==core::runtime::ResourceReleaseStatus::not_owner ||
        !f.resources.owns(existing.lease));
  CHECK(f.resources.active_count()==1 && second.opens==1);
  peer.shutdown(); CHECK(f.resources.active_count()==0);
}
void cancellation() {
  Fixture f;
  CHECK(f.runtime.submit({RuntimeAction::connect,10,++f.operation,0})==Code::accepted);
  CHECK(f.runtime.cancel(11,f.operation)==Code::permission_denied);
  CHECK(f.runtime.cancel(10,f.operation)==Code::accepted);
  f.runtime.service(); sdk::RuntimeCompletion out;
  CHECK(f.runtime.take_completion(out) && out.code==Code::cancelled);
  CHECK(f.driver.opens==0 && f.resources.active_count()==0);
  const auto c=f.execute(RuntimeAction::connect);
  f.driver.respond=false;
  const auto cancelled=++f.operation;
  CHECK(f.runtime.submit({RuntimeAction::sample,10,cancelled,c.session})==Code::accepted);
  f.runtime.service(); CHECK(f.driver.sends==1);
  CHECK(f.runtime.cancel(10,cancelled)==Code::accepted);
  f.runtime.service(); CHECK(f.runtime.take_completion(out) && out.code==Code::cancelled);
  CHECK(f.driver.reply(cancelled));
  CHECK(f.runtime.submit({RuntimeAction::sample,10,++f.operation,c.session})==Code::accepted);
  CHECK(f.runtime.cancel(10,cancelled)==Code::stale_activation);
  f.runtime.service(); CHECK(!f.runtime.take_completion(out)); // late response cannot complete new request
  CHECK(f.driver.reply(f.operation)); f.runtime.service();
  CHECK(f.runtime.take_completion(out) && out.code==Code::ok && out.request.operation==f.operation);
  CHECK(f.runtime.submit({RuntimeAction::sample,10,++f.operation,c.session})==Code::accepted);
  const auto before=f.driver.sends;
  f.runtime.shutdown(); CHECK(f.runtime.take_completion(out) && out.code==Code::cancelled);
  f.runtime.service(); CHECK(f.driver.sends==before && !f.driver.is_open());
  CHECK(f.channel.owner.load()==nullptr && f.resources.active_count()==0);
}
void timeouts() {
  Fixture f; auto c=f.execute(RuntimeAction::connect);
  f.driver.respond=false;
  CHECK(f.runtime.submit({RuntimeAction::sample,10,++f.operation,c.session})==Code::accepted);
  f.runtime.service(); CHECK(f.driver.reply(f.operation));
  f.clock.advance(51ms); f.runtime.service(); sdk::RuntimeCompletion out;
  CHECK(f.runtime.take_completion(out) && out.code==Code::timeout && !out.observed);
  CHECK(f.runtime.snapshot().state==RuntimeState::ready); // harmless timeout, not a restart
  f.driver.respond=true; CHECK(f.execute(RuntimeAction::sample,10,c.session).code==Code::ok);
  CHECK(f.driver.opens==1);
  f.clock.advance(101ms); f.runtime.service();
  CHECK(f.runtime.snapshot().state==RuntimeState::faulted);
  CHECK(f.runtime.snapshot().last_code==Code::timeout);
  CHECK(!f.driver.is_open() && f.resources.active_count()==1);
  CHECK(f.runtime.submit({RuntimeAction::connect,10,++f.operation,0})==Code::invalid_state);
  CHECK(f.execute(RuntimeAction::recover,10,c.session).code==Code::ok);
  CHECK(f.resources.active_count()==0 && f.runtime.snapshot().state==RuntimeState::idle);
  c=f.execute(RuntimeAction::connect);
  // Driver that overruns its declared bound must not publish a successful late sample.
  f.driver.receive_cost=60ms;
  CHECK(f.runtime.submit({RuntimeAction::sample,10,++f.operation,c.session})==Code::accepted);
  f.runtime.service(); CHECK(f.runtime.take_completion(out) && out.code!=Code::ok && !out.observed);
  CHECK(f.runtime.snapshot().state==RuntimeState::faulted);
  Fixture queued;
  CHECK(queued.runtime.submit({RuntimeAction::connect,1,1,0})==Code::accepted);
  queued.clock.advance(51ms); queued.runtime.service();
  CHECK(queued.runtime.take_completion(out) && out.code==Code::timeout);
  CHECK(queued.driver.opens==0 && queued.resources.active_count()==0);
}
void faults() {
  Fixture f; f.driver.open_result=core::transport::CanStatus::io_error;
  auto failed=f.execute(RuntimeAction::connect);
  CHECK(failed.code==Code::transport_error && !f.driver.is_open());
  CHECK(f.channel.owner.load()==nullptr && f.resources.active_count()==1);
  CHECK(f.execute(RuntimeAction::recover,10,failed.session).code==Code::ok);
  CHECK(f.resources.active_count()==0);
  f.driver.open_result=core::transport::CanStatus::ok;
  auto c=f.execute(RuntimeAction::connect);
  f.driver.next_send=core::transport::CanStatus::bus_off;
  CHECK(f.execute(RuntimeAction::sample,10,c.session).code==Code::transport_error);
  CHECK(!f.driver.is_open() && f.driver.queued()==0);
  CHECK(f.execute(RuntimeAction::recover,10,c.session).code==Code::ok);
  c=f.execute(RuntimeAction::connect);
  f.driver.next_receive=core::transport::CanStatus::io_error;
  CHECK(f.execute(RuntimeAction::sample,10,c.session).code==Code::transport_error);
  CHECK(f.execute(RuntimeAction::recover,10,c.session).code==Code::ok);
  c=f.execute(RuntimeAction::connect);
  f.clock.health=core::time::MonotonicClockStatus::discontinuity;
  f.runtime.service(); CHECK(f.runtime.snapshot().state==RuntimeState::faulted);
  CHECK(f.runtime.snapshot().last_code==Code::clock_fault && !f.driver.is_open());
  f.runtime.shutdown(); CHECK(f.resources.active_count()==0);
  // An unhealthy timestamp read immediately before TX must prevent that TX.
  Fixture late_clock; auto live=late_clock.execute(RuntimeAction::connect);
  CHECK(late_clock.runtime.submit({RuntimeAction::sample,10,++late_clock.operation,live.session})==Code::accepted);
  late_clock.clock.unavailable_at=late_clock.clock.reads+4;
  late_clock.runtime.service(); sdk::RuntimeCompletion no_tx;
  CHECK(late_clock.runtime.take_completion(no_tx) && no_tx.code==Code::clock_fault);
  CHECK(late_clock.driver.sends==0 && !late_clock.driver.is_open());
  // Teardown with a tampered/stale logical lease must not free somebody else's lease.
  Fixture stale;
  const auto active=stale.execute(RuntimeAction::connect);
  const auto original=stale.resources.acquire({core::runtime::ResourceClass::can_channel,1},10);
  CHECK(stale.resources.release(original.lease)==core::runtime::ResourceReleaseStatus::released);
  const auto other=stale.resources.acquire({core::runtime::ResourceClass::can_channel,1},99);
  CHECK(stale.execute(RuntimeAction::disconnect,10,active.session).code==Code::cleanup_failed);
  CHECK(stale.runtime.snapshot().state==RuntimeState::faulted);
  CHECK(!stale.runtime.snapshot().resources_held && stale.resources.owns(other.lease));
  CHECK(!stale.driver.is_open());
  CHECK(stale.execute(RuntimeAction::recover,10,active.session).code==Code::cleanup_failed);
  CHECK(stale.resources.owns(other.lease));
  // Terminal destruction closes and releases, including a pending request.
  mock::ManualClock clock; mock::MockChannel channel;
  mock::MockCan driver{channel,clock}; core::runtime::ResourceManager resources;
  {
    rt::Runtime lifetime{driver,clock,resources};
    CHECK(lifetime.submit({RuntimeAction::connect,1,1,0})==Code::accepted);
    lifetime.service(); CHECK(driver.is_open());
  }
  CHECK(!driver.is_open() && channel.owner.load()==nullptr && resources.active_count()==0);
}
void bounds() {
  Fixture f;
  CHECK(f.runtime.submit({RuntimeAction::connect,10,1,0})==Code::accepted);
  CHECK(f.runtime.submit({RuntimeAction::connect,11,2,0})==Code::busy);
  f.runtime.service();
  CHECK(f.runtime.submit({RuntimeAction::connect,11,2,0})==Code::busy); // unconsumed completion
  sdk::RuntimeCompletion c; CHECK(f.runtime.take_completion(c));
  CHECK(!f.runtime.take_completion(c));
  f.operation=1;
  f.driver.block_tx=true;
  CHECK(f.runtime.submit({RuntimeAction::sample,10,++f.operation,c.session})==Code::accepted);
  f.runtime.service(); CHECK(f.driver.send_attempts==1 && f.driver.sends==0);
  f.runtime.service(); CHECK(f.driver.send_attempts==2 && f.driver.sends==0);
  f.clock.advance(51ms); f.runtime.service(); sdk::RuntimeCompletion out;
  CHECK(f.runtime.take_completion(out) && out.code==Code::timeout);
  CHECK(f.driver.sends==0);
  f.driver.block_tx=false;
  core::transport::CanFrame unrelated{}; unrelated.identifier=0x77; unrelated.length=1;
  for (std::size_t i=0;i<16;++i) CHECK(f.driver.inject({unrelated,f.clock.read()}));
  const auto before=f.driver.receive_attempts; f.runtime.service();
  CHECK(f.driver.receive_attempts-before==8 && f.driver.queued()==8);
  for (std::size_t i=0;i<8;++i) CHECK(f.driver.inject({unrelated,f.clock.read()}));
  CHECK(!f.driver.inject({unrelated,f.clock.read()}));
  f.runtime.service(); CHECK(f.runtime.snapshot().last_code==Code::queue_overflow);
  CHECK(f.runtime.snapshot().lost_frames==1 && !f.driver.is_open());
  // Cancellation is not lost behind a saturated TX path or full RX ring.
  CHECK(f.execute(RuntimeAction::recover,10,c.session).code==Code::ok);
  c=f.execute(RuntimeAction::connect);
  f.driver.block_tx=true;
  CHECK(f.runtime.submit({RuntimeAction::sample,10,++f.operation,c.session})==Code::accepted);
  CHECK(f.runtime.cancel(10,f.operation)==Code::accepted);
  const auto attempts=f.driver.send_attempts; f.runtime.service();
  CHECK(f.runtime.take_completion(out) && out.code==Code::cancelled);
  CHECK(f.driver.send_attempts==attempts);
  // Bounded CORE resource table exhaustion does not touch a driver.
  Fixture full;
  for (std::uint32_t i=0;i<64;++i)
    CHECK(full.resources.acquire({core::runtime::ResourceClass::custom,i},20).status==core::runtime::ResourceAcquireStatus::acquired);
  CHECK(full.execute(RuntimeAction::connect).code==Code::capacity_exhausted);
  CHECK(full.driver.opens==0 && full.resources.active_count()==64);
}
void timestamps() {
  Fixture f; auto c=f.execute(RuntimeAction::connect); f.driver.respond=false;
  CHECK(f.runtime.submit({RuntimeAction::sample,10,++f.operation,c.session})==Code::accepted);
  f.runtime.service();
  auto frame=f.driver.last_sent; frame.identifier=0x601;
  auto wrong=f.clock.read(); wrong.domain={2};
  CHECK(f.driver.inject({frame,wrong})); f.runtime.service();
  sdk::RuntimeCompletion out; CHECK(f.runtime.take_completion(out) && out.code==Code::transport_error);
  CHECK(f.runtime.snapshot().state==RuntimeState::faulted);
  CHECK(f.execute(RuntimeAction::recover,10,c.session).code==Code::ok);
  c=f.execute(RuntimeAction::connect);
  CHECK(f.runtime.submit({RuntimeAction::sample,10,++f.operation,c.session})==Code::accepted);
  f.runtime.service(); frame=f.driver.last_sent; frame.identifier=0x601;
  auto old=f.clock.read(); old.value -= 1ms;
  CHECK(f.driver.inject({frame,old})); f.runtime.service();
  CHECK(!f.runtime.take_completion(out));
  CHECK(f.driver.reply(f.operation)); f.runtime.service();
  CHECK(f.runtime.take_completion(out) && out.code==Code::ok);
  CHECK(f.runtime.submit({RuntimeAction::sample,10,++f.operation,c.session})==Code::accepted);
  f.runtime.service(); frame=f.driver.last_sent; frame.identifier=0x601;
  auto future=f.clock.read(); future.value += 1ms;
  CHECK(f.driver.inject({frame,future})); f.runtime.service();
  CHECK(f.runtime.take_completion(out) && out.code==Code::clock_fault && !out.observed);
  rt::Config invalid; invalid.rx_budget=0;
  bool threw=false; try { rt::Runtime bad{f.driver,f.clock,f.resources,invalid}; }
  catch (const std::invalid_argument&) { threw=true; }
  CHECK(threw);
}
int main(int argc,char** argv) {
  if(argc!=2) return 2;
  const std::string name=argv[1];
  if(name=="lifecycle") lifecycle(); else if(name=="ownership") ownership();
  else if(name=="cancellation") cancellation(); else if(name=="timeouts") timeouts();
  else if(name=="faults") faults(); else if(name=="bounds") bounds();
  else if(name=="timestamps") timestamps(); else return 2;
  std::cout << "RUNTIME_MOCK_" << name << "=PASS\n";
}
