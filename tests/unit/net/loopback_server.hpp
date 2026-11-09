// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A tiny HTTP/1.1 server on 127.0.0.1 for the system-transport tests
// (URLSession, libcurl): one thread per connection, `Connection: close`,
// canned routes, and a log of the requests it served. Test-only.

#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cctype>
#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace hefesto::test {

struct ServedRequest {
  std::string path;
  std::map<std::string, std::string> headers;  // names lower-cased
};

class LoopbackServer {
 public:
  /// The bytes served at /blob (with range support) and /drop.
  std::string blob;

  LoopbackServer() {
    for (std::size_t i = 0; i < 300000; ++i) blob += static_cast<char>('a' + static_cast<char>(i % 26));
    listener_ = ::socket(AF_INET, SOCK_STREAM, 0);
    const int yes = 1;
    (void)::setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(0x7F000001U);  // 127.0.0.1
    address.sin_port = 0;
    (void)::bind(listener_, reinterpret_cast<const sockaddr*>(&address), sizeof address);
    (void)::listen(listener_, 16);
    socklen_t length = sizeof address;
    (void)::getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &length);
    port_ = ntohs(address.sin_port);
    acceptor_ = std::thread{[this] { accept_loop(); }};
  }
  LoopbackServer(const LoopbackServer&) = delete;
  LoopbackServer& operator=(const LoopbackServer&) = delete;
  ~LoopbackServer() {
    stop_ = true;
    acceptor_.join();
    for (auto& worker : workers_) worker.join();
    ::close(listener_);
  }

  [[nodiscard]] std::string url(std::string_view path) const {
    return "http://127.0.0.1:" + std::to_string(port_) + std::string{path};
  }
  [[nodiscard]] std::string localhost_url(std::string_view path) const {
    return "http://localhost:" + std::to_string(port_) + std::string{path};
  }
  [[nodiscard]] std::vector<ServedRequest> requests() const {
    const std::lock_guard guard{mutex_};
    return requests_;
  }
  /// /flaky answers 503 this many times before it answers 200.
  void set_flaky_failures(int count) { flaky_left_ = count; }

 private:
  void accept_loop() {
    while (!stop_) {
      pollfd wait{listener_, POLLIN, 0};
      if (::poll(&wait, 1, 20) <= 0) continue;
      const int client = ::accept(listener_, nullptr, nullptr);
      if (client < 0) continue;
#ifdef SO_NOSIGPIPE
      const int yes = 1;
      (void)::setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof yes);
#endif
      workers_.emplace_back([this, client] {
        serve(client);
        ::close(client);
      });
    }
  }

  static void send_all(int socket, std::string_view bytes) {
#ifdef MSG_NOSIGNAL
    constexpr int kFlags = MSG_NOSIGNAL;
#else
    constexpr int kFlags = 0;
#endif
    while (!bytes.empty()) {
      const auto sent = ::send(socket, bytes.data(), bytes.size(), kFlags);
      if (sent <= 0) return;
      bytes.remove_prefix(static_cast<std::size_t>(sent));
    }
  }

  static std::optional<ServedRequest> read_request(int socket) {
    std::string text;
    char buffer[4096];
    while (text.find("\r\n\r\n") == std::string::npos) {
      const auto got = ::recv(socket, buffer, sizeof buffer, 0);
      if (got <= 0) return std::nullopt;
      text.append(buffer, static_cast<std::size_t>(got));
    }
    ServedRequest request;
    const auto first_space = text.find(' ');
    request.path = text.substr(first_space + 1, text.find(' ', first_space + 1) - first_space - 1);
    std::size_t line = text.find("\r\n") + 2;
    while (line < text.size() && text.compare(line, 2, "\r\n") != 0) {
      const auto end = text.find("\r\n", line);
      const auto colon = text.find(':', line);
      std::string name = text.substr(line, colon - line);
      for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      request.headers[name] = text.substr(colon + 2, end - colon - 2);
      line = end + 2;
    }
    return request;
  }

  static std::string reply(int status, std::string_view reason, std::string_view body, std::string_view extra = {}) {
    return "HTTP/1.1 " + std::to_string(status) + " " + std::string{reason} + "\r\nContent-Length: " +
           std::to_string(body.size()) + "\r\nConnection: close\r\n" + std::string{extra} + "\r\n" + std::string{body};
  }

  static std::uint64_t range_start(const ServedRequest& request) {
    const auto it = request.headers.find("range");
    if (it == request.headers.end() || !it->second.starts_with("bytes=")) return 0;
    return std::stoull(it->second.substr(6));
  }

  std::string ranged_blob(const ServedRequest& request) const {
    const std::uint64_t from = range_start(request);
    if (from == 0) return reply(200, "OK", blob);
    if (from >= blob.size()) return reply(416, "Range Not Satisfiable", "", "Content-Range: bytes */300000\r\n");
    return reply(206, "Partial Content", std::string_view{blob}.substr(from),
                 "Content-Range: bytes " + std::to_string(from) + "-" + std::to_string(blob.size() - 1) + "/" +
                     std::to_string(blob.size()) + "\r\n");
  }

  // The first request to /drop sends half of the blob and closes the connection.
  void drop(int socket, const ServedRequest& request) {
    if (dropped_.exchange(true)) {
      send_all(socket, ranged_blob(request));
      return;
    }
    const std::string full = reply(200, "OK", blob);
    send_all(socket, std::string_view{full}.substr(0, full.size() - blob.size() / 2));
  }

  void serve(int socket) {
    const auto request = read_request(socket);
    if (!request) return;
    {
      const std::lock_guard guard{mutex_};
      requests_.push_back(*request);
    }
    const std::string& path = request->path;
    if (path == "/text") {
      send_all(socket, reply(200, "OK", "hello"));
    } else if (path == "/redirect") {
      send_all(socket, reply(302, "Found", "moved", "Location: /text\r\n"));
    } else if (path == "/cross-host") {
      send_all(socket, reply(302, "Found", "", "Location: " + localhost_url("/echo-auth") + "\r\n"));
    } else if (path == "/echo-auth") {
      const auto it = request->headers.find("authorization");
      send_all(socket, reply(200, "OK", it == request->headers.end() ? "none" : it->second));
    } else if (path == "/blob") {
      send_all(socket, ranged_blob(*request));
    } else if (path == "/norange") {
      send_all(socket, reply(200, "OK", blob));
    } else if (path == "/drop") {
      drop(socket, *request);
    } else if (path == "/flaky") {
      send_all(socket, flaky_left_-- > 0 ? reply(503, "Service Unavailable", "busy") : reply(200, "OK", "ok"));
    } else if (path == "/stall") {
      send_all(socket, "HTTP/1.1 200 OK\r\nContent-Length: 100\r\nConnection: close\r\n\r\npartial");
      std::this_thread::sleep_for(std::chrono::seconds{3});
    } else {
      send_all(socket, reply(404, "Not Found", "no such path"));
    }
  }

  int listener_ = -1;
  std::uint16_t port_ = 0;
  std::atomic<bool> stop_{false};
  std::atomic<bool> dropped_{false};
  std::atomic<int> flaky_left_{0};
  std::thread acceptor_;
  std::vector<std::thread> workers_;  // touched by the acceptor thread only, joined after it
  mutable std::mutex mutex_;
  std::vector<ServedRequest> requests_;
};

}  // namespace hefesto::test
