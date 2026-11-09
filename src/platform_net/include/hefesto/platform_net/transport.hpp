// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// One HTTP(S) GET, the only part of the HTTP Fetcher that differs per
// platform (http_fetcher.hpp). The Fetcher builds the request (headers,
// range, timeouts) and decides about retries, resume and diagnostics; a
// Transport performs one transfer, following redirects, and streams the
// final response to a TransferSink. Public so tests can script one.

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hefesto::platform_net {

struct TransferRequest {
  std::string url;
  std::vector<std::pair<std::string, std::string>> headers;
  /// > 0: ask for the bytes from this offset on (`Range: bytes=N-`).
  std::uint64_t range_from = 0;
  /// Whole transfer; 0 s: no limit.
  std::chrono::seconds total_timeout{0};
  /// No byte received for this long: TransferError::timeout.
  std::chrono::seconds stall_timeout{60};
  /// Accept http:// for the URL and redirects (tests); else https only.
  bool allow_plain_http = false;
};

/// The status line of the final response (after redirects).
struct TransferHead {
  int status = 0;
  /// Content-Length of this response; nullopt when unknown.
  std::optional<std::uint64_t> length;
};

enum class TransferError : std::uint8_t {
  none,
  timeout,      // connect, stall or total timeout
  resolve,      // host name not resolved, or no network
  connect,      // connection refused or unreachable
  interrupted,  // connection lost before the body was complete
  aborted,      // the sink stopped the transfer
  other,        // TLS, protocol, redirect policy, ...
};

struct TransferResult {
  int status = 0;  // final HTTP status; 0 when none was received
  TransferError error = TransferError::none;
  std::string message;  // the transport's description of `error`
};

/// Receives the final response. head() is called once before any body()
/// (also for an empty body); returning false from either stops the
/// transfer (TransferError::aborted). Calls come from one thread at a time.
class TransferSink {
 public:
  TransferSink() = default;
  TransferSink(const TransferSink&) = delete;
  TransferSink& operator=(const TransferSink&) = delete;
  virtual ~TransferSink() = default;

  virtual bool head(const TransferHead& head) = 0;
  virtual bool body(std::string_view bytes) = 0;
};

class Transport {
 public:
  Transport() = default;
  Transport(const Transport&) = delete;
  Transport& operator=(const Transport&) = delete;
  virtual ~Transport() = default;

  /// Performs one GET and returns when the transfer ended; never throws.
  [[nodiscard]] virtual TransferResult perform(const TransferRequest& request, TransferSink& sink) = 0;
};

/// This platform's transport: URLSession (Apple) or libcurl (Linux).
[[nodiscard]] std::unique_ptr<Transport> make_system_transport();

}  // namespace hefesto::platform_net
