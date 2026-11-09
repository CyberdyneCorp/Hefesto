// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The HTTP Fetcher's own logic over a scripted Transport (no network):
// schemes, GitHub headers, PDK-FETCH-001/002 texts, curl-like retries,
// resume and progress (hefesto-pdk-manager "Download integrity
// verification": HTTPS only, resumable downloads; hefesto-licensing D-13).

#include <catch2/catch_test_macros.hpp>

#include <deque>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/pdk/diag_codes.hpp"
#include "hefesto/platform_net/http_fetcher.hpp"
#include "temp_dir.hpp"

namespace net = hefesto::platform_net;
namespace pdk = hefesto::pdk;
namespace fs = std::filesystem;
using hefesto::test::read_file;
using hefesto::test::write_file;

namespace {

struct Reply {
  Reply(int status_code, std::string text = {}) : status(status_code), body(std::move(text)) {}  // NOLINT
  /// No HTTP response at all: the transfer failed with `kind`.
  static Reply failure(net::TransferError kind) {
    Reply out{0};
    out.error = kind;
    return out;
  }
  /// `deliver` bytes of the body arrive, then the transfer fails with `kind`.
  static Reply cut(int status_code, std::string text, net::TransferError kind, std::size_t deliver) {
    Reply out{status_code, std::move(text)};
    out.error = kind;
    out.deliver = deliver;
    return out;
  }

  int status = 200;
  std::string body;
  net::TransferError error = net::TransferError::none;
  std::size_t deliver = std::string::npos;  // bytes of `body` sent before `error`
};

/// Answers each perform() with the next scripted reply and records the requests.
class ScriptedTransport final : public net::Transport {
 public:
  explicit ScriptedTransport(std::deque<Reply>& replies, std::vector<net::TransferRequest>& seen)
      : replies_(replies), seen_(seen) {}

  net::TransferResult perform(const net::TransferRequest& request, net::TransferSink& sink) override {
    seen_.push_back(request);
    if (replies_.empty()) return {0, net::TransferError::other, "no scripted reply"};
    const Reply reply = replies_.front();
    replies_.pop_front();
    if (reply.status == 0) return {0, reply.error, "scripted failure"};
    if (!sink.head({reply.status, reply.body.size()})) {
      return {reply.status, net::TransferError::aborted, {}};
    }
    const std::string sent = reply.body.substr(0, reply.deliver);
    for (std::size_t at = 0; at < sent.size(); at += 4) {  // several chunks
      if (!sink.body(std::string_view{sent}.substr(at, 4))) return {reply.status, net::TransferError::aborted, {}};
    }
    return {reply.status, reply.error, reply.error == net::TransferError::none ? "" : "scripted failure"};
  }

 private:
  std::deque<Reply>& replies_;
  std::vector<net::TransferRequest>& seen_;
};

struct Harness {
  std::deque<Reply> replies;
  std::vector<net::TransferRequest> seen;
  std::unique_ptr<pdk::Fetcher> fetcher;

  explicit Harness(net::HttpFetcherOptions options = {}) {
    options.first_retry_delay = std::chrono::milliseconds{1};
    fetcher = net::make_http_fetcher(std::make_unique<ScriptedTransport>(replies, seen), std::move(options));
  }
};

std::optional<std::string> header(const net::TransferRequest& request, const std::string& name) {
  for (const auto& [key, value] : request.headers) {
    if (key == name) return value;
  }
  return std::nullopt;
}

constexpr const char* kAsset = "https://example.org/a/common.tar.zst";

}  // namespace

TEST_CASE("http fetcher: text, 404 and other HTTP errors keep the PDK-FETCH texts",
          "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  Harness h;
  h.replies = {{200, "{\"ok\":1}"}, {404, "nope"}, {403, "rate limited"}};
  const auto text = h.fetcher->get_text("https://example.org/manifest.json");
  REQUIRE(text.ok());
  CHECK(text.value() == "{\"ok\":1}");
  CHECK(h.seen.front().total_timeout == std::chrono::seconds{300});

  const auto missing = h.fetcher->get_text("https://example.org/missing.json");
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().code == pdk::codes::kFetchNotFound);
  CHECK(missing.error().text == "not found (HTTP 404): https://example.org/missing.json");

  const auto denied = h.fetcher->get_text("https://example.org/denied.json");
  REQUIRE_FALSE(denied.ok());
  CHECK(denied.error().code == pdk::codes::kFetch);
  CHECK(denied.error().text == "cannot download https://example.org/denied.json: HTTP 403");
  CHECK(h.seen.size() == 3);  // a 403 is not retried
}

TEST_CASE("http fetcher: only https and file URLs are fetched",
          "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  Harness h;
  for (const char* url : {"http://example.org/x", "ftp://example.org/x", "example.org/x"}) {
    const auto text = h.fetcher->get_text(url);
    REQUIRE_FALSE(text.ok());
    CHECK(text.error().code == pdk::codes::kFetch);
    CHECK(text.error().text.find("unsupported protocol") != std::string::npos);
  }
  hefesto::test::TempDir dir;
  CHECK_FALSE(h.fetcher->download("http://example.org/x", dir.path() / "x", {}).ok());
  CHECK(h.seen.empty());
}

TEST_CASE("http fetcher: the GitHub API gets its Accept header and the token, other hosts get neither",
          "[cap:hefesto-pdk-manager][req:GitHub releases source compatibility]") {
  net::HttpFetcherOptions options;
  options.github_token = "secret";
  Harness h{options};
  h.replies = {{200, "[]"}, {200, "{}"}};
  REQUIRE(h.fetcher->get_text("https://api.github.com/repos/o/r/releases?per_page=100&page=1").ok());
  REQUIRE(h.fetcher->get_text("https://github.com/o/r/releases/download/t/manifest.json").ok());
  CHECK(header(h.seen[0], "Accept") == "application/vnd.github+json");
  CHECK(header(h.seen[0], "Authorization") == "Bearer secret");
  CHECK_FALSE(header(h.seen[1], "Authorization"));
  CHECK_FALSE(header(h.seen[1], "Accept"));
}

TEST_CASE("http fetcher: transient failures are retried like curl --retry 3",
          "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  Harness h;
  h.replies = {{503, ""}, Reply::failure(net::TransferError::timeout), {504, ""}, {200, "fine"}};
  const auto text = h.fetcher->get_text("https://example.org/flaky");
  REQUIRE(text.ok());
  CHECK(text.value() == "fine");
  CHECK(h.seen.size() == 4);

  // Retries exhausted: the last failure is reported.
  h.replies = {{502, ""}, {502, ""}, {502, ""}, {502, ""}, {200, "never"}};
  h.seen.clear();
  const auto failed = h.fetcher->get_text("https://example.org/down");
  REQUIRE_FALSE(failed.ok());
  CHECK(failed.error().text == "cannot download https://example.org/down: HTTP 502");
  CHECK(h.seen.size() == 4);

  // Not transient: a refused connection fails at once, with its kind in the text.
  h.replies = {Reply::failure(net::TransferError::connect), {200, "never"}};
  h.seen.clear();
  const auto refused = h.fetcher->get_text("https://example.org/refused");
  REQUIRE_FALSE(refused.ok());
  CHECK(refused.error().text == "cannot download https://example.org/refused: cannot connect: scripted failure");
  CHECK(h.seen.size() == 1);
}

TEST_CASE("http fetcher: a download streams progress and resumes after a lost connection",
          "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  hefesto::test::TempDir dir;
  const fs::path to = dir.path() / "common.tar.zst";
  Harness h;
  const std::string bytes = "0123456789abcdefghij";
  // The first reply drops after 8 bytes; the retry asks for the rest (206).
  h.replies = {Reply::cut(200, bytes, net::TransferError::interrupted, 8), {206, bytes.substr(8)}};
  std::vector<std::pair<std::uint64_t, std::uint64_t>> events;
  REQUIRE(h.fetcher->download(kAsset, to, [&](std::uint64_t b, std::uint64_t t) { events.emplace_back(b, t); }).ok());
  CHECK(read_file(to) == bytes);
  REQUIRE(h.seen.size() == 2);
  CHECK(h.seen[0].range_from == 0);
  CHECK(h.seen[1].range_from == 8);
  CHECK(h.seen[1].total_timeout == std::chrono::seconds{0});  // downloads only have the stall timeout
  REQUIRE_FALSE(events.empty());
  CHECK(events.front() == std::pair<std::uint64_t, std::uint64_t>{4, 20});
  CHECK(events[2] == std::pair<std::uint64_t, std::uint64_t>{12, 20});  // resumed: offset + this reply's length
  CHECK(events.back() == std::pair<std::uint64_t, std::uint64_t>{20, 20});
}

TEST_CASE("http fetcher: a resume answered with 200 restarts the file, 416 leaves a complete one",
          "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  hefesto::test::TempDir dir;
  const fs::path to = dir.path() / "asset";
  Harness h;
  write_file(to, "stale-prefix");
  h.replies = {{200, "fresh content"}};
  REQUIRE(h.fetcher->download(kAsset, to, {}).ok());
  CHECK(h.seen.back().range_from == 12);
  CHECK(read_file(to) == "fresh content");

  // curl -C -: "range not satisfiable" on a resume means nothing is left to get.
  h.replies = {{416, "<html>error page</html>"}};
  REQUIRE(h.fetcher->download(kAsset, to, {}).ok());
  CHECK(read_file(to) == "fresh content");

  // A failed download keeps the partial file for the next run; the text names the status.
  h.replies = {{404, "not here"}};
  const auto missing = h.fetcher->download("https://example.org/gone", to, {});
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().code == pdk::codes::kFetch);
  CHECK(missing.error().text == "cannot download https://example.org/gone: HTTP 404");
  CHECK(read_file(to) == "fresh content");
}

TEST_CASE("http fetcher: file URLs are read from disk, with resume and not-found",
          "[cap:hefesto-pdk-manager][req:PDK catalog]") {
  hefesto::test::TempDir dir;
  write_file(dir.path() / "cat alog" / "manifest.json", "{\"pdk\":\"sky130\"}");
  write_file(dir.path() / "asset.bin", "abcdefghij");
  Harness h;
  const std::string base = "file://" + dir.path().string();
  const auto text = h.fetcher->get_text(base + "/cat%20alog/manifest.json");
  REQUIRE(text.ok());
  CHECK(text.value() == "{\"pdk\":\"sky130\"}");
  const auto missing = h.fetcher->get_text(base + "/nothing.json");
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().code == pdk::codes::kFetchNotFound);
  CHECK(missing.error().text == "not found: " + base + "/nothing.json");

  const fs::path to = dir.path() / "copy.bin";
  write_file(to, "abcd");  // a partial earlier copy
  std::uint64_t last = 0;
  REQUIRE(h.fetcher->download(base + "/asset.bin", to, [&](std::uint64_t b, std::uint64_t) { last = b; }).ok());
  CHECK(read_file(to) == "abcdefghij");
  CHECK(last == 10);
  CHECK_FALSE(h.fetcher->download(base + "/absent.bin", to, {}).ok());
  CHECK(h.seen.empty());  // no transport involved
}

namespace {

/// A transport that restarts the response once (URLSession may retry an
/// idempotent GET itself after a lost connection): two heads, one sink.
class RestartingTransport final : public net::Transport {
 public:
  net::TransferResult perform(const net::TransferRequest& /*request*/, net::TransferSink& sink) override {
    (void)sink.head({200, 10});
    (void)sink.body("lost");
    (void)sink.head({200, 10});
    (void)sink.body("0123456789");
    return {200, net::TransferError::none, {}};
  }
};

}  // namespace

// Regression: a second head() reopened the already open file, which failed
// the download with "cannot write"; the file now restarts with the response.
TEST_CASE("http fetcher: a response restarted by the transport restarts the file",
          "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  hefesto::test::TempDir dir;
  auto fetcher = net::make_http_fetcher(std::make_unique<RestartingTransport>(), {});
  const fs::path to = dir.path() / "asset";
  const auto status = fetcher->download(kAsset, to, {});
  INFO((status.ok() ? std::string{} : status.error().text));
  REQUIRE(status.ok());
  CHECK(read_file(to) == "0123456789");
}
