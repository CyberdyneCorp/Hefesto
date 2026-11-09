// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Host-side helpers of the PDK manager: the unsupported and curl-process
// fetchers, the zstd decompressors and the local install host.

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

#include "hefesto/diag/codes.hpp"
#include "hefesto/pdk/diag_codes.hpp"
#include "hefesto/pdk/manager.hpp"

#include <zstd.h>

namespace hefesto::pdk {

namespace {

class UnsupportedFetcher final : public Fetcher {
 public:
  diag::Outcome<std::string> get_text(std::string_view url) override { return unsupported(url); }
  diag::Status download(std::string_view url, const platform::Path& /*to*/,
                        const std::function<void(std::uint64_t, std::uint64_t)>& /*progress*/) override {
    return unsupported(url);
  }

 private:
  static diag::Diagnostic unsupported(std::string_view url) {
    return diag::make(diag::codes::kPlatformUnsupported, "downloads are not available here: " + std::string{url});
  }
};

std::string first_line(const std::string& text) {
  return text.substr(0, text.find('\n'));
}

std::uint64_t size_or_zero(const platform::Path& path) {
  std::error_code ec;
  const std::uint64_t size = std::filesystem::file_size(path, ec);
  return ec ? 0 : size;
}

// Reports the growing size of a file every 200 ms on a worker thread until
// destroyed. The worker is joined in the destructor, so no call happens after.
class FileGrowthReporter {
 public:
  FileGrowthReporter(platform::Path path, const std::function<void(std::uint64_t, std::uint64_t)>& progress)
      : path_(std::move(path)), progress_(progress) {
    if (progress_) {
      worker_ = std::thread{[this] { poll(); }};
    }
  }
  FileGrowthReporter(const FileGrowthReporter&) = delete;
  FileGrowthReporter& operator=(const FileGrowthReporter&) = delete;
  ~FileGrowthReporter() {
    {
      const std::scoped_lock lock{mutex_};
      stop_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable()) {
      worker_.join();
    }
  }

 private:
  void poll() {
    std::uint64_t last = 0;
    std::unique_lock lock{mutex_};
    while (!wake_.wait_for(lock, std::chrono::milliseconds{200}, [this] { return stop_; })) {
      const std::uint64_t size = size_or_zero(path_);
      if (size != last) {
        last = size;
        progress_(size, 0);
      }
    }
  }

  platform::Path path_;
  const std::function<void(std::uint64_t, std::uint64_t)>& progress_;
  std::mutex mutex_;
  std::condition_variable wake_;
  bool stop_ = false;
  std::thread worker_;
};

// curl through the platform ProcessRunner. HTTPS uses curl's system trust
// store; `--proto =https,file` refuses plain HTTP and other schemes.
class ProcessFetcher final : public Fetcher {
 public:
  ProcessFetcher(platform::ProcessRunner& runner, std::optional<std::string> token)
      : runner_(runner), token_(std::move(token)) {}

  diag::Outcome<std::string> get_text(std::string_view url) override {
    platform::ProcessRequest request{"curl", {"-sSL", "--proto", "=https,file", "--max-time", "300", "-w", "\n%{http_code}"},
                                     std::nullopt, {}};
    add_github_headers(request, url);
    request.args.emplace_back(url);
    auto output = runner_.run(request);
    if (!output) {
      return fetch_error(url, output.error().text);
    }
    return parse_response(url, output.value());
  }

  diag::Status download(std::string_view url, const platform::Path& to,
                        const std::function<void(std::uint64_t, std::uint64_t)>& progress) override {
    platform::ProcessRequest request{
        "curl",
        {"-fsSL", "--proto", "=https,file", "--retry", "3", "-C", "-", "-o", to.string(), std::string{url}},
        std::nullopt,
        {}};
    auto output = [&] {
      const FileGrowthReporter reporter{to, progress};
      return runner_.run(request);
    }();
    if (!output) {
      return fetch_error(url, output.error().text);
    }
    if (output.value().exit_code != 0) {
      return fetch_error(url, "curl exit " + std::to_string(output.value().exit_code) + ": " +
                                  first_line(output.value().stderr_data));
    }
    if (progress) {
      const std::uint64_t size = size_or_zero(to);
      progress(size, size);
    }
    return diag::Done{};
  }

 private:
  static diag::Diagnostic fetch_error(std::string_view url, const std::string& why) {
    return diag::make(codes::kFetch, "cannot download " + std::string{url} + ": " + why);
  }

  void add_github_headers(platform::ProcessRequest& request, std::string_view url) const {
    if (!url.starts_with("https://api.github.com/")) {
      return;
    }
    request.args.insert(request.args.end(), {"-H", "Accept: application/vnd.github+json"});
    if (token_ && !token_->empty()) {
      request.args.insert(request.args.end(), {"-K", "-"});
      request.stdin_data = "header = \"Authorization: Bearer " + *token_ + "\"\n";
    }
  }

  static diag::Outcome<std::string> parse_response(std::string_view url, const platform::ProcessOutput& out) {
    if (out.exit_code == 37 && url.starts_with("file:")) {
      return diag::make(codes::kFetchNotFound, "not found: " + std::string{url});
    }
    if (out.exit_code != 0) {
      return fetch_error(url, "curl exit " + std::to_string(out.exit_code) + ": " + first_line(out.stderr_data));
    }
    const auto newline = out.stdout_data.rfind('\n');
    const std::string code = newline == std::string::npos ? std::string{} : out.stdout_data.substr(newline + 1);
    std::string body = newline == std::string::npos ? out.stdout_data : out.stdout_data.substr(0, newline);
    if (code == "404") {
      return diag::make(codes::kFetchNotFound, "not found (HTTP 404): " + std::string{url});
    }
    if (!code.empty() && code != "000" && code.front() != '2') {
      return fetch_error(url, "HTTP " + code);
    }
    return body;
  }

  platform::ProcessRunner& runner_;
  std::optional<std::string> token_;
};

// Streaming zstd decoder (the pinned hefesto::zstd library); stops as soon as
// the output exceeds the limit and removes the partial output on any failure.
class LibraryDecompressor final : public Decompressor {
 public:
  diag::Status decompress(const platform::Path& in, const platform::Path& out, std::uint64_t max_bytes) override {
    diag::Status status = decode(in, out, max_bytes);
    if (!status.ok()) {
      std::error_code ec;
      std::filesystem::remove(out, ec);
    }
    return status;
  }

 private:
  static diag::Status decode(const platform::Path& in, const platform::Path& out, std::uint64_t max_bytes) {
    std::ifstream source{in, std::ios::binary};
    std::ofstream sink{out, std::ios::binary | std::ios::trunc};
    if (!source || !sink) {
      return diag::make(codes::kInstallIo, "cannot open " + in.string() + " or " + out.string());
    }
    std::unique_ptr<ZSTD_DStream, decltype(&ZSTD_freeDStream)> stream{ZSTD_createDStream(), &ZSTD_freeDStream};
    std::vector<char> input(ZSTD_DStreamInSize());
    std::vector<char> output(ZSTD_DStreamOutSize());
    std::uint64_t written = 0;
    std::size_t last = 1;
    while (source) {
      source.read(input.data(), static_cast<std::streamsize>(input.size()));
      ZSTD_inBuffer in_buffer{input.data(), static_cast<std::size_t>(source.gcount()), 0};
      while (in_buffer.pos < in_buffer.size) {
        ZSTD_outBuffer out_buffer{output.data(), output.size(), 0};
        last = ZSTD_decompressStream(stream.get(), &out_buffer, &in_buffer);
        if (ZSTD_isError(last) != 0) {
          return diag::make(codes::kBadArchive, in.filename().string() + ": " + ZSTD_getErrorName(last));
        }
        written += out_buffer.pos;
        if (written > max_bytes) {
          return diag::make(codes::kUnpackLimit, "decompressed size exceeds the limit of " + std::to_string(max_bytes));
        }
        sink.write(output.data(), static_cast<std::streamsize>(out_buffer.pos));
      }
    }
    if (last != 0) {
      return diag::make(codes::kBadArchive, in.filename().string() + " is truncated");
    }
    sink.flush();
    return sink ? diag::Status{diag::Done{}} : diag::make(codes::kInstallIo, "cannot write " + out.string());
  }
};

class LocalInstallHost final : public InstallHost {
 public:
  diag::Status make_symlink(const platform::Path& target, const platform::Path& link) override {
    std::error_code ec;
    std::filesystem::create_symlink(target, link, ec);
    if (ec) {
      return diag::make(codes::kInstallIo, "cannot create symbolic link " + link.string() + ": " + ec.message());
    }
    return diag::Done{};
  }

  diag::Outcome<std::optional<platform::Path>> read_symlink(const platform::Path& link) override {
    std::error_code ec;
    if (!std::filesystem::is_symlink(std::filesystem::symlink_status(link, ec))) {
      return std::optional<platform::Path>{};
    }
    auto target = std::filesystem::read_symlink(link, ec);
    if (ec) {
      return diag::make(diag::codes::kPlatformRead, "cannot read symbolic link " + link.string());
    }
    return std::optional<platform::Path>{std::move(target)};
  }

  std::optional<std::string> environment(std::string_view name) override {
    const char* value = std::getenv(std::string{name}.c_str());  // NOLINT(concurrency-mt-unsafe)
    if (value == nullptr || *value == '\0') {
      return std::nullopt;
    }
    return std::string{value};
  }

  platform::Path home_directory() override { return environment("HOME").value_or(std::string{}); }
};

}  // namespace

std::unique_ptr<Fetcher> make_unsupported_fetcher() {
  return std::make_unique<UnsupportedFetcher>();
}

std::unique_ptr<Fetcher> make_process_fetcher(platform::ProcessRunner& runner, std::optional<std::string> github_token) {
  return std::make_unique<ProcessFetcher>(runner, std::move(github_token));
}

bool have_zstd_library() noexcept {
  return true;
}

std::unique_ptr<Decompressor> make_default_decompressor(platform::ProcessRunner& runner) {
  (void)runner;  // kept for API stability: the decoder no longer runs a process
  return std::make_unique<LibraryDecompressor>();
}

std::unique_ptr<InstallHost> make_local_install_host() {
  return std::make_unique<LocalInstallHost>();
}

}  // namespace hefesto::pdk
