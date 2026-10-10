#include "ecu/app/host/manifest.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <utility>

namespace ecu::app::host {
namespace {
using sdk::Code;
sdk::Result invalid(std::string detail) {
  return {Code::invalid_manifest, {}, std::move(detail), 0, 0, {}};
}
std::string_view trim(std::string_view text) {
  const auto start = text.find_first_not_of(" \t");
  if (start == std::string_view::npos) return {};
  return text.substr(start, text.find_last_not_of(" \t") - start + 1);
}
bool number(std::string_view text, std::uint16_t& out) noexcept {
  if (text.empty() || text.size() > 5 || (text.size() > 1 && text.front() == '0')) return false;
  unsigned int n = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') return false;
    n = n * 10U + static_cast<unsigned int>(c - '0');
  }
  if (n > (std::numeric_limits<std::uint16_t>::max)()) return false;
  out = static_cast<std::uint16_t>(n);
  return true;
}
bool parts(std::string_view text, std::uint16_t* values, const std::size_t count) noexcept {
  for (std::size_t i = 0; i < count; ++i) {
    const auto end = text.find('.');
    if ((i + 1 == count) != (end == std::string_view::npos)) return false;
    const auto token = text.substr(0, end);
    if (!number(token, values[i])) return false;
    if (end != std::string_view::npos) text.remove_prefix(end + 1);
  }
  return true;
}
}  // namespace

bool valid_name(const std::string_view value) noexcept {
  const auto alnum = [](const char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };
  if (value.empty() || value.size() > 64 || !alnum(value.front()) || !alnum(value.back()) ||
      value.find("..") != std::string_view::npos) return false;
  for (const char c : value) if (!alnum(c) && c != '.' && c != '-' && c != '_') return false;
  return true;
}
bool valid_text(const std::string_view value) noexcept {
  if (value.size() > sdk::kMaxTextBytes) return false;
  // ASCII only in this transport proof; no locale-dependent classification.
  return std::all_of(value.begin(), value.end(), [](const unsigned char c) { return c >= 32 && c <= 126; });
}
bool parse_version(const std::string_view value, sdk::Version& version) noexcept {
  std::array<std::uint16_t, 3> v{};
  if (!parts(value, v.data(), v.size())) return false;
  version = {v[0], v[1], v[2]};
  return true;
}
std::string version_string(const sdk::Version& v) {
  return std::to_string(v.major) + '.' + std::to_string(v.minor) + '.' + std::to_string(v.patch);
}

sdk::Result validate_manifest(const sdk::Manifest& m) {
  if (m.schema != 1 || !valid_name(m.id)) return invalid("schema or module id");
  if (m.requires_services.empty() || m.requires_services.size() > sdk::kMaxItems ||
      m.operations.empty() || m.operations.size() > sdk::kMaxItems) return invalid("item count");
  std::set<std::string> caps;
  for (const auto& cap : m.requires_services) {
    if (!valid_name(cap) || !caps.insert(cap).second) return invalid("capability name or duplicate");
  }
  std::set<std::string> ids;
  for (const auto& op : m.operations) {
    if (!valid_name(op.id) || !valid_name(op.service) || !ids.insert(op.id).second)
      return invalid("operation name or duplicate");
    if (caps.count(op.service) == 0) return invalid("operation uses undeclared capability");
  }
  return {Code::ok, m.id, {}, 0, 0, {}};
}

ParsedManifest parse_manifest(const std::string_view text) {
  ParsedManifest parsed;
  auto fail = [&](const char* detail) { parsed.result = invalid(detail); return parsed; };
  if (text.empty() || text.size() > sdk::kMaxManifestBytes) return fail("manifest size");
  for (const unsigned char c : text) {
    if (c != '\n' && c != '\r' && c != '\t' && (c < 32 || c > 126)) return fail("manifest encoding");
  }
  std::set<std::string> keys;
  std::size_t position = 0;
  std::size_t lines = 0;
  while (position < text.size()) {
    if (++lines > 64) return fail("line count");
    const auto end = text.find('\n', position);
    auto line = text.substr(position, end == std::string_view::npos ? text.size() - position : end - position);
    position = end == std::string_view::npos ? text.size() : end + 1;
    if (line.size() > sdk::kMaxLineBytes) return fail("line size");
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.find('\r') != std::string_view::npos) return fail("embedded carriage return");
    line = trim(line);
    if (line.empty() || line.front() == '#') continue;
    const auto equal = line.find('=');
    if (equal == std::string_view::npos) return fail("expected key=value");
    const std::string key{trim(line.substr(0, equal))};
    const auto value = trim(line.substr(equal + 1));
    if (value.empty() || !keys.insert(key).second) return fail("empty value or duplicate key");
    auto& m = parsed.manifest;
    if (key == "schema") {
      if (!number(value, m.schema)) return fail("schema number");
    } else if (key == "id") {
      m.id = value;
    } else if (key == "version") {
      if (!parse_version(value, m.version)) return fail("version number");
    } else if (key == "api") {
      std::array<std::uint16_t, 2> v{};
      if (!parts(value, v.data(), v.size())) return fail("api number");
      m.requires_api = {v[0], v[1]};
    } else if (key == "requires") {
      auto remaining = value;
      while (true) {
        const auto comma = remaining.find(',');
        const auto item = trim(remaining.substr(0, comma));
        if (item.empty() || m.requires_services.size() == sdk::kMaxItems) return fail("capability list");
        m.requires_services.emplace_back(item);
        if (comma == std::string_view::npos) break;
        remaining.remove_prefix(comma + 1);
      }
    } else if (key.compare(0, 10, "operation.") == 0) {
      if (m.operations.size() == sdk::kMaxItems) return fail("operation count");
      m.operations.push_back({key.substr(10), std::string{value}});
    } else {
      return fail("unknown field");
    }
  }
  for (const char* required : {"schema", "id", "version", "api", "requires"}) {
    if (keys.count(required) == 0) return fail("missing required field");
  }
  parsed.result = validate_manifest(parsed.manifest);
  return parsed;
}

const char* code_name(const Code code) noexcept {
  switch (code) {
#define ECU_APP_CODE(x) case Code::x: return #x
    ECU_APP_CODE(ok); ECU_APP_CODE(invalid_manifest); ECU_APP_CODE(incompatible_api);
    ECU_APP_CODE(missing_capability); ECU_APP_CODE(duplicate_id); ECU_APP_CODE(capacity_exhausted);
    ECU_APP_CODE(not_found); ECU_APP_CODE(already_active); ECU_APP_CODE(not_active);
    ECU_APP_CODE(stale_activation); ECU_APP_CODE(active_module); ECU_APP_CODE(busy); ECU_APP_CODE(version_conflict);
    ECU_APP_CODE(unknown_operation); ECU_APP_CODE(permission_denied); ECU_APP_CODE(invalid_input);
    ECU_APP_CODE(operation_failed); ECU_APP_CODE(counter_exhausted); ECU_APP_CODE(io_error);
    ECU_APP_CODE(invalid_command);
#undef ECU_APP_CODE
  }
  return "unknown_code";
}
}  // namespace ecu::app::host
