// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The platform-independent part of the HTTP Fetcher: schemes, GitHub
// headers, retries, resume and the PDK-FETCH-001/002 diagnostics. The
// transfer itself is a Transport (URLSession or libcurl).

#include "hefesto/platform_net/http_fetcher.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>
#include <thread>
#include <utility>

#include "file_url.hpp"
#include "hefesto/pdk/diag_codes.hpp"

namespace hefesto::platform_net {

namespace {

namespace stdfs = std::filesystem;
using ProgressFn = std::function<void(std::uint64_t, std::uint64_t)>;

constexpr std::string_view kGitHubApi = "https://api.github.com/";
// A catalog or release listing is small; this bounds a misbehaving server.
constexpr std::size_t kMaxTextBytes = std::size_t{64} * 1024 * 1024;

enum class Scheme : std::uint8_t { https, http, file, unsupported };

Scheme scheme_of(std::string_view url, bool allow_plain_http) {
  if (url.starts_with("https://")) return Scheme::https;
  if (url.starts_with("file://")) return Scheme::file;
  if (allow_plain_http && url.starts_with("http://")) return Scheme::http;
  return Scheme::unsupported;
}

diag::Diagnostic fetch_error(std::string_view url, const std::string& why) {
  return diag::make(pdk::codes::kFetch, "cannot download " + std::string{url} + ": " + why);
}

diag::Diagnostic unsupported_scheme(std::string_view url) {
  return fetch_error(url, "unsupported protocol (only https:// and file:// URLs are fetched)");
}

bool success(int status) {
  return status >= 200 && status < 300;
}

// curl --retry: an HTTP 408, 429, 500, 502, 503 or 504 reply is transient.
bool transient_status(int status) {
  return status == 408 || status == 429 || status == 500 || status == 502 || status == 503 || status == 504;
}

bool transient_error(TransferError error) {
  return error == TransferError::timeout || error == TransferError::resolve || error == TransferError::interrupted;
}

std::string error_kind(TransferError error) {
  switch (error) {
    case TransferError::timeout: return "timeout";
    case TransferError::resolve: return "cannot resolve host";
    case TransferError::connect: return "cannot connect";
    case TransferError::interrupted: return "connection lost";
    case TransferError::aborted: return "aborted";
    case TransferError::none:
    case TransferError::other: break;
  }
  return "transfer failed";
}

std::string transfer_failure(const TransferResult& result) {
  return result.message.empty() ? error_kind(result.error) : error_kind(result.error) + ": " + result.message;
}

std::uint64_t size_or_zero(const platform::Path& path) {
  std::error_code ec;
  const std::uint64_t size = stdfs::file_size(path, ec);
  return ec ? 0 : size;
}

// One attempt: its outcome and whether a retry may succeed.
struct Attempt {
  diag::Status status;
  bool transient = false;
};

class TextSink final : public TransferSink {
 public:
  bool head(const TransferHead& /*head*/) override { return true; }
  bool body(std::string_view bytes) override {
    if (text_.size() + bytes.size() > kMaxTextBytes) {
      too_large_ = true;
      return false;
    }
    text_.append(bytes);
    return true;
  }
  [[nodiscard]] bool too_large() const noexcept { return too_large_; }
  [[nodiscard]] std::string take() { return std::move(text_); }

 private:
  std::string text_;
  bool too_large_ = false;
};

// Writes a 2xx body to `to`: appended after a 206 reply to the range
// request, from the start after a 200. Other replies' bodies are dropped.
class FileSink final : public TransferSink {
 public:
  FileSink(platform::Path to, std::uint64_t offset, const ProgressFn& progress)
      : to_(std::move(to)), offset_(offset), progress_(progress) {}

  bool head(const TransferHead& head) override {
    if (out_.is_open()) out_.close();  // a transport that restarted the response
    if (!success(head.status)) return true;
    const bool resumed = head.status == 206 && offset_ > 0;
    written_ = resumed ? offset_ : 0;
    total_ = head.length ? written_ + *head.length : 0;
    out_.open(to_, std::ios::binary | (resumed ? std::ios::app : std::ios::trunc));
    write_failed_ = !out_;
    return !write_failed_;
  }

  bool body(std::string_view bytes) override {
    if (!out_.is_open()) return true;
    out_.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out_) {
      write_failed_ = true;
      return false;
    }
    written_ += bytes.size();
    if (progress_) progress_(written_, total_);
    return true;
  }

  /// Closes the file; false when a write failed.
  bool finish() {
    if (out_.is_open()) {
      out_.close();
      write_failed_ = write_failed_ || !out_;
    }
    return !write_failed_;
  }

 private:
  platform::Path to_;
  std::uint64_t offset_;
  const ProgressFn& progress_;
  std::ofstream out_;
  std::uint64_t written_ = 0;
  std::uint64_t total_ = 0;
  bool write_failed_ = false;
};

class HttpFetcher final : public pdk::Fetcher {
 public:
  HttpFetcher(std::unique_ptr<Transport> transport, HttpFetcherOptions options)
      : transport_(std::move(transport)), options_(std::move(options)) {}

  diag::Outcome<std::string> get_text(std::string_view url) override {
    const Scheme scheme = scheme_of(url, options_.allow_plain_http);
    if (scheme == Scheme::file) return detail::read_file_url(url);
    if (scheme == Scheme::unsupported) return unsupported_scheme(url);
    std::string text;
    diag::Status status = with_retries([&] { return text_attempt(url, text); });
    if (!status) return status.error();
    return text;
  }

  diag::Status download(std::string_view url, const platform::Path& to, const ProgressFn& progress) override {
    const Scheme scheme = scheme_of(url, options_.allow_plain_http);
    if (scheme == Scheme::file) return detail::copy_file_url(url, to, progress);
    if (scheme == Scheme::unsupported) return unsupported_scheme(url);
    diag::Status status = with_retries([&] { return download_attempt(url, to, progress); });
    if (status && progress) {
      const std::uint64_t size = size_or_zero(to);
      progress(size, size);
    }
    return status;
  }

 private:
  template <typename Once>
  diag::Status with_retries(Once once) const {
    Attempt attempt = once();
    auto delay = options_.first_retry_delay;
    for (int retry = 0; retry < options_.retries && attempt.transient; ++retry) {
      std::this_thread::sleep_for(delay);
      delay *= 2;
      attempt = once();
    }
    return std::move(attempt.status);
  }

  [[nodiscard]] TransferRequest request(std::string_view url) const {
    TransferRequest out;
    out.url = std::string{url};
    out.stall_timeout = options_.stall_timeout;
    out.allow_plain_http = options_.allow_plain_http;
    if (url.starts_with(kGitHubApi)) {
      out.headers.emplace_back("Accept", "application/vnd.github+json");
      if (options_.github_token && !options_.github_token->empty()) {
        out.headers.emplace_back("Authorization", "Bearer " + *options_.github_token);
      }
    }
    return out;
  }

  Attempt text_attempt(std::string_view url, std::string& text) {
    TransferRequest req = request(url);
    req.total_timeout = options_.text_timeout;
    TextSink sink;
    const TransferResult result = transport_->perform(req, sink);
    if (sink.too_large()) return {fetch_error(url, "response larger than 64 MiB"), false};
    if (result.error != TransferError::none) {
      return {fetch_error(url, transfer_failure(result)), transient_error(result.error)};
    }
    if (result.status == 404) {
      return {diag::make(pdk::codes::kFetchNotFound, "not found (HTTP 404): " + std::string{url}), false};
    }
    if (!success(result.status)) {
      return {fetch_error(url, "HTTP " + std::to_string(result.status)), transient_status(result.status)};
    }
    text = sink.take();
    return {diag::Done{}, false};
  }

  Attempt download_attempt(std::string_view url, const platform::Path& to, const ProgressFn& progress) {
    TransferRequest req = request(url);
    req.range_from = size_or_zero(to);
    FileSink sink{to, req.range_from, progress};
    const TransferResult result = transport_->perform(req, sink);
    if (!sink.finish()) return {fetch_error(url, "cannot write " + to.string()), false};
    if (result.error != TransferError::none) {
      return {fetch_error(url, transfer_failure(result)), transient_error(result.error)};
    }
    // curl -C -: a 416 reply to a resume means the file is already complete.
    if (result.status == 416 && req.range_from > 0) return {diag::Done{}, false};
    if (!success(result.status)) {
      return {fetch_error(url, "HTTP " + std::to_string(result.status)), transient_status(result.status)};
    }
    return {diag::Done{}, false};
  }

  std::unique_ptr<Transport> transport_;
  HttpFetcherOptions options_;
};

}  // namespace

std::unique_ptr<pdk::Fetcher> make_http_fetcher(std::unique_ptr<Transport> transport, HttpFetcherOptions options) {
  return std::make_unique<HttpFetcher>(std::move(transport), std::move(options));
}

std::unique_ptr<pdk::Fetcher> make_http_fetcher(HttpFetcherOptions options) {
  return make_http_fetcher(make_system_transport(), std::move(options));
}

std::function<std::unique_ptr<pdk::Fetcher>(platform::PlatformServices&)> http_fetcher_factory(
    std::optional<std::string> github_token) {
  return [token = std::move(github_token)](platform::PlatformServices& /*services*/) {
    HttpFetcherOptions options;
    options.github_token = token;
    return make_http_fetcher(std::move(options));
  };
}

}  // namespace hefesto::platform_net
