#pragma once

#include "ecu/app/sdk/contracts.hpp"
#include <string_view>

namespace ecu::app::host {
struct ParsedManifest {
  sdk::Result result;
  sdk::Manifest manifest;
};
// Bounded proof format only: flat key=value records, no scripts or file references.
[[nodiscard]] ParsedManifest parse_manifest(std::string_view text);
[[nodiscard]] sdk::Result validate_manifest(const sdk::Manifest& manifest);
[[nodiscard]] bool valid_name(std::string_view value) noexcept;
[[nodiscard]] bool valid_text(std::string_view value) noexcept;
[[nodiscard]] bool parse_version(std::string_view value, sdk::Version& version) noexcept;
[[nodiscard]] std::string version_string(const sdk::Version& version);
[[nodiscard]] const char* code_name(sdk::Code code) noexcept;
}  // namespace ecu::app::host
