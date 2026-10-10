#pragma once
#include "ecu/app/host/manifest.hpp"
#include <string>

// Local CLI input adapter, not a signed installer or an executable package loader.
[[nodiscard]] ecu::app::host::ParsedManifest read_package(const std::string& directory);
