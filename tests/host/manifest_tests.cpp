#include "ecu/app/host/manifest.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

namespace host = ecu::app::host;
namespace sdk = ecu::app::sdk;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (false)

const std::string source =
    "schema=1\nid=org.example.one\nversion=1.2.3\napi=1.0\nrequires=demo.echo\noperation.ping=demo.echo\n";
std::string changed(const std::string& from, const std::string& to) {
  auto result = source;
  result.replace(result.find(from), from.size(), to);
  return result;
}
void rejected(const std::string& text) { CHECK(host::parse_manifest(text).result.code == sdk::Code::invalid_manifest); }

int main() {
  const auto parsed = host::parse_manifest(source);
  CHECK(parsed.result.ok());
  CHECK(parsed.manifest.id == "org.example.one");
  CHECK((parsed.manifest.version == sdk::Version{1, 2, 3}));
  CHECK(parsed.manifest.operations.size() == 1);
  CHECK(parsed.manifest.operations[0].service == "demo.echo");
  CHECK(host::parse_manifest("# comment\n\n" + source).result.ok());
  CHECK(host::parse_manifest(changed("schema=1", "schema = 1")).result.ok());
  CHECK(host::parse_manifest(source.substr(0, source.size() - 1)).result.ok());
  std::string crlf;
  for (char c : source) { if (c == '\n') crlf += '\r'; crlf += c; }
  CHECK(host::parse_manifest(crlf).result.ok());
  CHECK(host::parse_manifest(changed("version=1.2.3", "version=65535.65535.65535")).result.ok());
  for (const auto* field : {"schema=1\n", "id=org.example.one\n", "version=1.2.3\n", "api=1.0\n", "requires=demo.echo\n"})
    rejected(changed(field, ""));
  for (const auto* version : {"1", "1.0", "1.0.0.0", "-1.0.0", "+1.0.0", "01.0.0", "65536.0.0", "1.0.0-beta", "1. 0.0"})
    rejected(changed("version=1.2.3", std::string{"version="} + version));
  rejected("");
  rejected(source + "schema=1\n");
  rejected(source + "operation.ping=demo.echo\n");
  rejected(source + "grant=all\n");
  rejected(source + "entrypoint=run.py\n");
  rejected(changed("schema=1", "schema=2"));
  rejected(changed("id=org.example.one", "id=../escape"));
  rejected(changed("id=org.example.one", "id=UPPERCASE"));
  rejected(changed("id=org.example.one", "id="));
  rejected(changed("id=org.example.one", "id=" + std::string(65, 'x')));
  rejected(changed("api=1.0", "api=1.0.0"));
  rejected(changed("requires=demo.echo", "requires=demo.echo,demo.echo"));
  rejected(changed("requires=demo.echo", "requires=demo.echo,"));
  rejected(changed("requires=demo.echo", "requires=,demo.echo"));
  rejected(changed("operation.ping=demo.echo", "operation.ping=undeclared.service"));
  rejected(changed("operation.ping=demo.echo", "operation.=demo.echo"));
  rejected(changed("schema=1", "schema\r=1"));
  rejected(source + std::string(1, '\0'));
  rejected(source + std::string(1, static_cast<char>(0xff)));
  rejected(source + "#" + std::string(512, 'x'));
  rejected(source + std::string(8193, 'x'));
  rejected(source + std::string(65, '\n'));
  auto many = source;
  for (int i = 0; i < 16; ++i) many += "operation.extra" + std::to_string(i) + "=demo.echo\n";
  rejected(many);
  auto typed = parsed.manifest;
  typed.operations[0].service = "not.declared";
  CHECK(!host::validate_manifest(typed).ok());
  typed = parsed.manifest;
  typed.operations.push_back(typed.operations[0]);
  CHECK(!host::validate_manifest(typed).ok());
  sdk::Version untouched{3, 2, 1};
  CHECK(!host::parse_version("bad", untouched));
  CHECK((untouched == sdk::Version{3, 2, 1}));
  CHECK(host::version_string(untouched) == "3.2.1");
  CHECK(host::valid_text("quoted \" text \\"));
  CHECK(!host::valid_text("line\nbreak"));
  CHECK(!host::valid_text(std::string(513, 'x')));
  std::cout << "MANIFEST_CONTRACT=PASS\n";
}
