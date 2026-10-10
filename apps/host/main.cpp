#include "ecu/app/host/host.hpp"
#include "ecu/app/host/manifest.hpp"
#include "package_reader.hpp"

#include <algorithm>
#include <charconv>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>

namespace {
namespace sdk = ecu::app::sdk;
namespace host = ecu::app::host;

void quote(const std::string& value) {
  // Every string published by the proof is printable ASCII, except a diagnostic
  // path supplied by the operator, which is never echoed. JSON quoting is exact.
  std::cout << std::quoted(value);
}
void emit_result(const sdk::Result& result) {
  std::cout << "{\"status\":"; quote(host::code_name(result.code));
  std::cout << ",\"module\":"; quote(result.module);
  std::cout << ",\"detail\":"; quote(result.detail);
  std::cout << ",\"generation\":" << result.generation;
  std::cout << ",\"operation_id\":" << result.operation_id;
  std::cout << ",\"payload\":"; quote(result.payload);
  std::cout << '}';
}
void emit_catalog(const host::Host& platform) {
  std::cout << "{\"status\":\"ok\",\"modules\":[";
  bool first = true;
  for (const auto& m : platform.catalog()) {
    if (!first) std::cout << ',';
    first = false;
    std::cout << "{\"id\":"; quote(m.manifest.id);
    std::cout << ",\"version\":"; quote(host::version_string(m.manifest.version));
    std::cout << ",\"state\":"; quote(m.state == sdk::State::active ? "active" : "disabled");
    std::cout << ",\"generation\":" << m.generation;
    std::cout << ",\"completed_operations\":" << m.completed_operations;
    std::cout << ",\"operations\":[";
    bool first_op = true;
    for (const auto& op : m.manifest.operations) {
      if (!first_op) std::cout << ',';
      first_op = false;
      std::cout << "{\"id\":"; quote(op.id);
      std::cout << ",\"service\":"; quote(op.service);
      std::cout << ",\"allowed\":"
                << (std::find(m.allowed_operations.begin(), m.allowed_operations.end(), op.id) != m.allowed_operations.end() ? "true" : "false") << '}';
    }
    std::cout << "],\"last_operation\":";
    if (m.last_operation.operation_id == 0) std::cout << "null";
    else emit_result(m.last_operation);
    std::cout << '}';
  }
  std::cout << "]}\n";
}

bool end_of_command(std::istringstream& input) {
  input >> std::ws;
  return input.eof();
}
bool generation_number(const std::string& text, std::uint64_t& value) {
  if (text.empty() || (text.size() > 1 && text.front() == '0')) return false;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && value != 0;
}
// Consume oversize commands without allocating an unbounded string.
bool read_line(std::string& line, bool& oversized) {
  line.clear(); oversized = false;
  char c = 0;
  bool any = false;
  while (std::cin.get(c)) {
    any = true;
    if (c == '\n') break;
    if (line.size() < 2048) line += c;
    else oversized = true;
  }
  if (!line.empty() && line.back() == '\r') line.pop_back();
  return any;
}

int run_host() {
  // Two deliberately harmless proof capabilities. Neither touches hardware,
  // loads code, starts subprocesses or knows a package ID/operation name.
  host::Host platform{{
      {"demo.echo", [](std::string_view input) { return host::ServiceResult{true, std::string{input}}; }},
      {"demo.fail", [](std::string_view) { return host::ServiceResult{false, "intentional demo failure"}; }},
  }};
  std::string line;
  bool oversized = false;
  while (read_line(line, oversized)) {
    sdk::Result response;
    response.code = sdk::Code::invalid_command;
    response.detail = "invalid command or arguments";
    bool printable = true;
    for (const unsigned char c : line) if (c != '\t' && (c < 32 || c > 126)) printable = false;
    if (oversized || !printable) {
      emit_result(response); std::cout << '\n' << std::flush; continue;
    }
    std::istringstream input{line};
    std::string command, id, arg, op, payload;
    input >> command;
    if (command.empty() || command.front() == '#') continue;
    if (command == "quit" && end_of_command(input)) {
      response.code = sdk::Code::ok; response.detail = "host stopped; in-memory catalog discarded";
      emit_result(response); std::cout << '\n'; return 0;
    }
    if (command == "list" && end_of_command(input)) {
      emit_catalog(platform); std::cout << std::flush; continue;
    }
    if (command == "install" && (input >> std::quoted(arg)) && end_of_command(input)) {
      auto package = read_package(arg);
      response = package.result.ok() ? platform.install(package.manifest) : package.result;
    } else if (command == "replace" && (input >> std::quoted(arg) >> id) && end_of_command(input)) {
      sdk::Version expected;
      if (host::parse_version(id, expected)) {
        auto package = read_package(arg);
        response = package.result.ok() ? platform.replace(package.manifest, expected) : package.result;
      }
    } else if (command == "remove" && (input >> id) && end_of_command(input)) {
      response = platform.remove(id);
    } else if (command == "activate" && (input >> id) && end_of_command(input)) {
      response = platform.activate(id);
    } else if ((command == "allow" || command == "deny") && (input >> id >> op) && end_of_command(input)) {
      response = command == "allow" ? platform.allow(id, op) : platform.deny(id, op);
    } else if (command == "deactivate" && (input >> id >> arg) && end_of_command(input)) {
      std::uint64_t generation = 0;
      if (generation_number(arg, generation)) response = platform.deactivate(id, generation);
    } else if (command == "run" && (input >> id >> arg >> op >> std::quoted(payload)) && end_of_command(input)) {
      std::uint64_t generation = 0;
      if (generation_number(arg, generation)) response = platform.invoke(id, generation, op, payload);
    }
    emit_result(response); std::cout << '\n' << std::flush;
  }
  return std::cin.bad() ? 2 : 0;
}
}  // namespace

int main(const int argc, char** argv) {
  if (argc == 2 && std::string{argv[1]} == "--help") {
    std::cout << "ECU Platform APP - offline host proof, API 1.1\n"
                 "No GUI, network, native plugins, hardware or persistent installation.\n"
                 "Commands (stdin, one per line; result is one JSON object):\n"
                 "  install \"package-directory\"\n  list\n"
                 "  allow module-id operation\n  deny module-id operation\n"
                 "  activate module-id\n  run module-id generation operation \"ASCII input\"\n"
                 "  deactivate module-id generation\n"
                 "  replace \"package-directory\" expected-version\n  remove module-id\n  quit\n";
    return 0;
  }
  if (argc != 1) { std::cerr << "Use --help or supply commands on stdin.\n"; return 2; }
  try { return run_host(); }
  catch (const std::exception&) { std::cerr << "Host initialization or resource failure.\n"; return 2; }
}
