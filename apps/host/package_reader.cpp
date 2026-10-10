#include "package_reader.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace {
ecu::app::host::ParsedManifest failure(const char* detail) {
  ecu::app::host::ParsedManifest parsed;
  parsed.result.code = ecu::app::sdk::Code::io_error;
  parsed.result.detail = detail;
  return parsed;
}
}
ecu::app::host::ParsedManifest read_package(const std::string& directory) {
  namespace fs = std::filesystem;
  try {
    std::error_code error;
    const auto root = fs::u8path(directory);
    const auto root_status = fs::symlink_status(root, error);
    if (error || !fs::is_directory(root_status)) return failure("package must be a non-symlink directory");
    const auto manifest = root / "manifest.ecu";
    const auto file_status = fs::symlink_status(manifest, error);
    if (error || !fs::is_regular_file(file_status)) return failure("manifest must be a non-symlink regular file");
    // Read into a bounded buffer even when a local file changes after the check.
    // This is NOT a race-free sandbox for concurrent hostile filesystem changes.
    std::array<char, ecu::app::sdk::kMaxManifestBytes + 1> bytes{};
    std::ifstream input(manifest, std::ios::binary);
    if (!input) return failure("cannot open manifest");
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (input.bad()) return failure("cannot read manifest");
    const auto count = static_cast<std::size_t>(input.gcount());
    return ecu::app::host::parse_manifest(std::string_view{bytes.data(), count});
  } catch (const fs::filesystem_error&) {
    return failure("filesystem error");
  }
}
