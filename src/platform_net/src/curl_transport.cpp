// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The libcurl Transport (Linux; hefesto-licensing D-13): the pinned static
// libcurl with OpenSSL, certificate verification against the system trust
// store, HTTP(S) only, redirects followed by libcurl (which never forwards
// the Authorization header to another host).

#include <curl/curl.h>

#include <array>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

#include "hefesto/platform_net/http_fetcher.hpp"
#include "hefesto/platform_net/transport.hpp"
#include "hefesto/version.hpp"

namespace hefesto::platform_net {

namespace {

constexpr long kMaxRedirects = 10;
constexpr long kConnectTimeoutSeconds = 30;

void global_init() {
  static std::once_flag once;
  std::call_once(once, [] { (void)curl_global_init(CURL_GLOBAL_DEFAULT); });
}

TransferError classify(CURLcode code) {
  switch (code) {
    case CURLE_OK: return TransferError::none;
    case CURLE_OPERATION_TIMEDOUT: return TransferError::timeout;
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_COULDNT_RESOLVE_PROXY: return TransferError::resolve;
    case CURLE_COULDNT_CONNECT: return TransferError::connect;
    case CURLE_PARTIAL_FILE:
    case CURLE_RECV_ERROR:
    case CURLE_SEND_ERROR:
    case CURLE_GOT_NOTHING:
    case CURLE_HTTP2_STREAM: return TransferError::interrupted;
    case CURLE_WRITE_ERROR: return TransferError::aborted;
    default: return TransferError::other;
  }
}

// Forwards the final response to the sink. With redirects followed,
// libcurl does not pass the bodies of 3xx replies; they are skipped here too.
struct Receiver {
  using Clock = std::chrono::steady_clock;

  CURL* easy = nullptr;
  TransferSink* sink = nullptr;
  std::chrono::seconds stall_timeout{60};
  bool head_sent = false;
  bool stalled = false;
  curl_off_t received = 0;
  Clock::time_point last_activity = Clock::now();

  int status() const {
    long code = 0;
    (void)curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &code);
    return static_cast<int>(code);
  }

  bool send_head() {
    head_sent = true;
    curl_off_t length = -1;
    (void)curl_easy_getinfo(easy, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &length);
    TransferHead head{status(), std::nullopt};
    if (length >= 0) head.length = static_cast<std::uint64_t>(length);
    return sink->head(head);
  }

  static std::size_t write(char* data, std::size_t size, std::size_t count, void* user) {
    auto* self = static_cast<Receiver*>(user);
    const std::size_t bytes = size * count;
    const int code = self->status();
    if (code >= 300 && code < 400) return bytes;
    if (!self->head_sent && !self->send_head()) return 0;
    return self->sink->body(std::string_view{data, bytes}) ? bytes : 0;
  }

  // The stall timeout like URLSession's request timeout: no byte received
  // for stall_timeout fails the transfer (libcurl's low-speed check averages
  // over several seconds, so a trickle never trips it).
  static int on_progress(void* user, curl_off_t /*dltotal*/, curl_off_t dlnow, curl_off_t /*ultotal*/,
                         curl_off_t /*ulnow*/) {
    auto* self = static_cast<Receiver*>(user);
    const auto now = Clock::now();
    if (dlnow != self->received) {
      self->received = dlnow;
      self->last_activity = now;
    }
    self->stalled = now - self->last_activity > self->stall_timeout;
    return self->stalled ? 1 : 0;
  }
};

using Easy = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
using Headers = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;

Headers header_list(const TransferRequest& request) {
  Headers list{nullptr, &curl_slist_free_all};
  for (const auto& [name, value] : request.headers) {
    const std::string line = name + ": " + value;
    curl_slist* grown = curl_slist_append(list.get(), line.c_str());
    if (grown != nullptr) {
      (void)list.release();
      list.reset(grown);
    }
  }
  return list;
}

// Seconds as the `long` libcurl takes (no cast: `long` is the rep on LP64).
long as_long(std::chrono::seconds seconds) {
  return std::chrono::duration_cast<std::chrono::duration<long>>(seconds).count();
}

void set_options(CURL* easy, const TransferRequest& request, curl_slist* headers) {
  const char* protocols = request.allow_plain_http ? "http,https" : "https";
  const std::string agent = "Hefesto/" + std::string{version()};
  (void)curl_easy_setopt(easy, CURLOPT_URL, request.url.c_str());
  (void)curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, protocols);
  (void)curl_easy_setopt(easy, CURLOPT_REDIR_PROTOCOLS_STR, protocols);
  (void)curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
  (void)curl_easy_setopt(easy, CURLOPT_MAXREDIRS, kMaxRedirects);
  (void)curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
  (void)curl_easy_setopt(easy, CURLOPT_USERAGENT, agent.c_str());  // copied by libcurl
  (void)curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSeconds);
  (void)curl_easy_setopt(easy, CURLOPT_TIMEOUT, as_long(request.total_timeout));
  (void)curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);
  if (request.range_from > 0) {
    const std::string range = std::to_string(request.range_from) + "-";
    (void)curl_easy_setopt(easy, CURLOPT_RANGE, range.c_str());  // copied by libcurl
  }
}

TransferResult result_of(CURLcode code, Receiver& receiver, const char* message) {
  TransferResult result{receiver.status(), classify(code), {}};
  if (receiver.stalled) {
    result.error = TransferError::timeout;
    result.message = "no data received for " + std::to_string(receiver.stall_timeout.count()) + " s";
    return result;
  }
  if (result.error == TransferError::none && !receiver.head_sent && !receiver.send_head()) {
    result.error = TransferError::aborted;
  }
  if (code != CURLE_OK) result.message = message[0] != '\0' ? message : curl_easy_strerror(code);
  return result;
}

class CurlTransport final : public Transport {
 public:
  CurlTransport() { global_init(); }

  TransferResult perform(const TransferRequest& request, TransferSink& sink) override {
    Easy easy{curl_easy_init(), &curl_easy_cleanup};
    if (!easy) return {0, TransferError::other, "cannot start libcurl"};
    const Headers headers = header_list(request);
    set_options(easy.get(), request, headers.get());
    Receiver receiver;
    receiver.easy = easy.get();
    receiver.sink = &sink;
    receiver.stall_timeout = request.stall_timeout;
    std::array<char, CURL_ERROR_SIZE> message{};
    (void)curl_easy_setopt(easy.get(), CURLOPT_WRITEFUNCTION, &Receiver::write);
    (void)curl_easy_setopt(easy.get(), CURLOPT_WRITEDATA, &receiver);
    (void)curl_easy_setopt(easy.get(), CURLOPT_XFERINFOFUNCTION, &Receiver::on_progress);
    (void)curl_easy_setopt(easy.get(), CURLOPT_XFERINFODATA, &receiver);
    (void)curl_easy_setopt(easy.get(), CURLOPT_NOPROGRESS, 0L);
    (void)curl_easy_setopt(easy.get(), CURLOPT_ERRORBUFFER, message.data());
    const CURLcode code = curl_easy_perform(easy.get());
    return result_of(code, receiver, message.data());
  }
};

}  // namespace

std::unique_ptr<Transport> make_system_transport() {
  return std::make_unique<CurlTransport>();
}

std::string_view http_backend_name() noexcept {
  return "libcurl";
}

}  // namespace hefesto::platform_net
