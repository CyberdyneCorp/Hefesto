// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// This platform's HTTP transport (URLSession on macOS, libcurl on Linux;
// hefesto-licensing D-13) through the HTTP Fetcher against a loopback HTTP
// server: redirects, 404, range resume, a dropped connection, retries of
// HTTP 503 and the stall timeout. Plain HTTP is allowed only by the test
// option; TLS itself is exercised by the network test (label `network`).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "hefesto/pdk/diag_codes.hpp"
#include "hefesto/platform_net/http_fetcher.hpp"
#include "loopback_server.hpp"
#include "temp_dir.hpp"

namespace net = hefesto::platform_net;
namespace pdk = hefesto::pdk;
namespace fs = std::filesystem;
using hefesto::test::LoopbackServer;
using hefesto::test::read_file;
using hefesto::test::write_file;

namespace {

std::unique_ptr<pdk::Fetcher> loopback_fetcher() {
  net::HttpFetcherOptions options;
  options.allow_plain_http = true;
  options.first_retry_delay = std::chrono::milliseconds{10};
  options.stall_timeout = std::chrono::seconds{1};
  return net::make_http_fetcher(std::move(options));
}

class TextSink final : public net::TransferSink {
 public:
  bool head(const net::TransferHead& head) override {
    status = head.status;
    return true;
  }
  bool body(std::string_view bytes) override {
    text.append(bytes);
    return true;
  }
  int status = 0;
  std::string text;
};

}  // namespace

TEST_CASE("system transport: text, redirects and 404", "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  LoopbackServer server;
  auto fetcher = loopback_fetcher();
  INFO(net::http_backend_name());
  const auto text = fetcher->get_text(server.url("/text"));
  INFO((text.ok() ? std::string{} : text.error().text));
  REQUIRE(text.ok());
  CHECK(text.value() == "hello");
  const auto redirected = fetcher->get_text(server.url("/redirect"));
  REQUIRE(redirected.ok());
  CHECK(redirected.value() == "hello");  // the 3xx body is not part of the text
  const auto missing = fetcher->get_text(server.url("/missing"));
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().code == pdk::codes::kFetchNotFound);
  CHECK(missing.error().text == "not found (HTTP 404): " + server.url("/missing"));
  CHECK(server.requests().front().headers.at("user-agent").starts_with("Hefesto/"));
}

TEST_CASE("system transport: plain http is refused without the test option",
          "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  LoopbackServer server;
  auto fetcher = net::make_http_fetcher();
  const auto text = fetcher->get_text(server.url("/text"));
  REQUIRE_FALSE(text.ok());
  CHECK(text.error().code == pdk::codes::kFetch);
  CHECK(server.requests().empty());
}

TEST_CASE("system transport: the Authorization header is not sent to another host on a redirect",
          "[cap:hefesto-pdk-manager][req:GitHub releases source compatibility]") {
  LoopbackServer server;
  auto transport = net::make_system_transport();
  net::TransferRequest request;
  request.url = server.url("/echo-auth");
  request.headers = {{"Authorization", "Bearer secret"}};
  request.allow_plain_http = true;
  TextSink same;
  REQUIRE(transport->perform(request, same).error == net::TransferError::none);
  CHECK(same.text == "Bearer secret");
  request.url = server.url("/cross-host");  // 127.0.0.1 -> localhost
  TextSink other;
  const auto result = transport->perform(request, other);
  INFO(result.message);
  REQUIRE(result.error == net::TransferError::none);
  CHECK(other.status == 200);
  CHECK(other.text == "none");
}

TEST_CASE("system transport: downloads resume with a range request and stream progress",
          "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  hefesto::test::TempDir dir;
  LoopbackServer server;
  auto fetcher = loopback_fetcher();
  const fs::path to = dir.path() / "blob";
  write_file(to, server.blob.substr(0, 1000));  // an earlier partial download
  std::uint64_t first = 0;
  std::uint64_t last = 0;
  std::uint64_t total = 0;
  const auto status = fetcher->download(server.url("/blob"), to, [&](std::uint64_t b, std::uint64_t t) {
    if (first == 0) first = b;
    last = b;
    total = t;
  });
  INFO((status.ok() ? std::string{} : status.error().text));
  REQUIRE(status.ok());
  CHECK(read_file(to) == server.blob);
  CHECK(server.requests().back().headers.at("range") == "bytes=1000-");
  CHECK(first > 1000);
  CHECK(last == server.blob.size());
  CHECK(total == server.blob.size());

  // Complete already: the server answers 416, the file is kept as is.
  REQUIRE(fetcher->download(server.url("/blob"), to, {}).ok());
  CHECK(read_file(to) == server.blob);

  // A server without range support restarts the file.
  write_file(to, "garbage");
  REQUIRE(fetcher->download(server.url("/norange"), to, {}).ok());
  CHECK(read_file(to) == server.blob);
}

TEST_CASE("system transport: a dropped connection and HTTP 503 are retried, the download resumed",
          "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  hefesto::test::TempDir dir;
  LoopbackServer server;
  auto fetcher = loopback_fetcher();
  const fs::path to = dir.path() / "blob";
  const auto status = fetcher->download(server.url("/drop"), to, {});
  INFO((status.ok() ? std::string{} : status.error().text));
  REQUIRE(status.ok());
  CHECK(read_file(to) == server.blob);
  // Retried by the Fetcher (with a range request for the bytes on disk) or,
  // on macOS, by URLSession itself for an idempotent GET before any byte
  // reached the Fetcher: either way a second request completes the file.
  const auto requests = server.requests();
  CHECK(requests.size() >= 2);

  server.set_flaky_failures(2);
  const auto text = fetcher->get_text(server.url("/flaky"));
  REQUIRE(text.ok());
  CHECK(text.value() == "ok");
  server.set_flaky_failures(10);
  const auto failed = fetcher->get_text(server.url("/flaky"));
  REQUIRE_FALSE(failed.ok());
  CHECK(failed.error().text == "cannot download " + server.url("/flaky") + ": HTTP 503");
}

TEST_CASE("system transport: a stalled transfer times out", "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  hefesto::test::TempDir dir;
  LoopbackServer server;
  net::HttpFetcherOptions options;
  options.allow_plain_http = true;
  options.stall_timeout = std::chrono::seconds{1};
  options.retries = 0;
  auto fetcher = net::make_http_fetcher(std::move(options));
  const auto status = fetcher->download(server.url("/stall"), dir.path() / "stall", {});
  REQUIRE_FALSE(status.ok());
  INFO(status.error().text);
  CHECK(status.error().code == pdk::codes::kFetch);
  CHECK(status.error().text.starts_with("cannot download " + server.url("/stall") + ": timeout"));
}
