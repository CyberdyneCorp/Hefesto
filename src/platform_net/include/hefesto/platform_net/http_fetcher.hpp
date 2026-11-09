// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The in-process HTTPS Fetcher of the PDK manager (capability
// hefesto-pdk-manager, requirement "Download integrity verification";
// hefesto-licensing, requirement "Third-party libraries for cryptography,
// networking and data formats", decision D-13): URLSession on macOS and
// libcurl on Linux, behind one C++ interface so that no framework or
// libcurl type reaches the core (hefesto-architecture "Platform services
// abstraction"). It replaces the curl-process Fetcher in the desktop app and
// hefesto-cli.
//
// Behavior, shared by both transports:
//   - schemes: https (certificate validation against the system trust
//     store), file:// read directly from disk (local catalogs); anything
//     else fails with PDK-FETCH-001 (plain http only with
//     HttpFetcherOptions::allow_plain_http, for tests against a loopback
//     server)
//   - redirects are followed (at most 10, https only; the Authorization
//     header is never sent to another host)
//   - requests to https://api.github.com/ carry
//     `Accept: application/vnd.github+json` and, when a token is set,
//     `Authorization: Bearer <token>`
//   - get_text: HTTP 404 -> PDK-FETCH-002 "not found (HTTP 404): <url>";
//     any other failure -> PDK-FETCH-001 "cannot download <url>: <why>",
//     e.g. "HTTP 503", "timeout: ...", "cannot resolve host: ..."
//   - download: resumes a partial file with a range request (a 200 reply
//     restarts it, a 416 reply on a resume means it is complete and is left
//     to the manager's digest check), streams progress (bytes in the file,
//     total or 0) from the transfer thread, and keeps the partial file on
//     failure so a later run resumes it
//   - transient failures (timeout, host not resolved, connection lost
//     mid-transfer, HTTP 408, 429, 500, 502, 503, 504) are retried
//     `retries` times, waiting first_retry_delay and doubling it, like
//     `curl --retry 3`; a retried download resumes from the bytes on disk
//
// Owner: INTEGRATION (platform services); PDK (Fetcher contract).

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/pdk/manager.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/platform_net/transport.hpp"

namespace hefesto::platform_net {

struct HttpFetcherOptions {
  /// Sent as a Bearer token to https://api.github.com/ only (GITHUB_TOKEN).
  std::optional<std::string> github_token;
  /// Tests only: also accept http:// URLs (a loopback test server).
  bool allow_plain_http = false;
  /// Whole get_text request, redirects and retries' waits excluded (curl --max-time 300).
  std::chrono::seconds text_timeout{300};
  /// No byte received for this long fails the request as a timeout.
  std::chrono::seconds stall_timeout{60};
  /// Retries of a transient failure (curl --retry 3).
  int retries = 3;
  /// Wait before the first retry, doubled after each one (curl: 1 s).
  std::chrono::milliseconds first_retry_delay{1000};
};

/// The Fetcher over this platform's HTTPS client (URLSession or libcurl).
[[nodiscard]] std::unique_ptr<pdk::Fetcher> make_http_fetcher(HttpFetcherOptions options = {});
/// The same Fetcher over another transport (tests).
[[nodiscard]] std::unique_ptr<pdk::Fetcher> make_http_fetcher(std::unique_ptr<Transport> transport,
                                                              HttpFetcherOptions options);

/// A pdk::CommandHosts::fetcher factory making make_http_fetcher with `github_token`.
[[nodiscard]] std::function<std::unique_ptr<pdk::Fetcher>(platform::PlatformServices&)> http_fetcher_factory(
    std::optional<std::string> github_token);

/// "URLSession" or "libcurl" (reports, tests).
[[nodiscard]] std::string_view http_backend_name() noexcept;

}  // namespace hefesto::platform_net
