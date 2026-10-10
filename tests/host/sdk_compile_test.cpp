#include "ecu/app/sdk/contracts.hpp"
#include <type_traits>
// This target links ONLY ECU::app_sdk: no host, Core, OS or GUI libraries.
static_assert(std::is_copy_constructible_v<ecu::app::sdk::Manifest>);
int main() {
  ecu::app::sdk::Result result;
  return result.ok() && ecu::app::sdk::kHostApi.major == 1 ? 0 : 1;
}
