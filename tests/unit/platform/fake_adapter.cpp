// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto_fake_adapter: a stand-in hefesto_engine_v1 plugin for the worker
// tests ("a test build whose adapter, when a deck asks it to, tries ...";
// simulation-engine-embedding "Worker confinement"). hefesto-simd loads it
// with --adapter=<path>, after confinement, like the real plugin. No ngspice.
//
// Deck lines "* fake: <directive>" act during load or run:
//   sleep-load <ms>   load takes <ms> milliseconds        (Large deck load)
//   net <port>        connect to 127.0.0.1:<port>         (Worker confinement)
//   write <path>      create <path>                       (Worker confinement)
//   spawn             start /bin/sh                       (Worker confinement)
//   stderr <text>     an error-stream line during load    (Callback contract)
//   points <n>        points per vector (default 8)
//   crash             abort() in run                      (Crash containment)
//   exit              the controlled-exit path in run     (Controlled exit)
//   stop              SIGSTOP itself in run               (Stuck worker)
//
// The results depend on the deck, on the generator seeded by `setseed`, and
// on the circuits and plots left over from earlier jobs, so a worker that
// skipped `setseed` or the clear between jobs produces different vectors
// than a fresh one (the reuse sequence of design D9).
//
// Owner: PLATFORM (tests).

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "abi/hefesto_engine_v1.h"

extern char** environ;  // NOLINT(readability-redundant-declaration)

struct hefesto_engine {
  hefesto_engine_callbacks callbacks{};
  std::uint64_t random = 0;
  int circuits = 0;
  int plots = 0;
  std::vector<std::string> deck;
  std::size_t points = 8;
  std::vector<double> time;
  std::vector<double> out;
  std::thread runner;
  std::string error;
};

namespace {

std::atomic<bool> g_created{false};
constexpr std::string_view kDirective = "* fake: ";

void say(hefesto_engine* e, const std::string& text, bool error = false) {
  if (e->callbacks.on_output != nullptr) {
    e->callbacks.on_output(e->callbacks.user, text.c_str(), text.size(), error ? 1 : 0);
  }
}

void state(hefesto_engine* e, int s) {
  if (e->callbacks.on_state != nullptr) {
    e->callbacks.on_state(e->callbacks.user, s);
  }
}

// SplitMix64.
double next(std::uint64_t& s) {
  s += 0x9E3779B97F4A7C15ULL;
  std::uint64_t z = s;
  z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
  z ^= z >> 31U;
  return static_cast<double>(z >> 11U) * 0x1.0p-53;
}

std::string denied(int error) { return std::string{"denied ("} + std::strerror(error) + ")"; }

std::string try_connect(int port) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return denied(errno);
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<std::uint16_t>(port));
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  const int rc = ::connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof address);  // NOLINT
  const int error = errno;
  ::close(fd);
  return rc == 0 ? std::string{"connected"} : denied(error);
}

std::string try_write(const std::string& path) {
  const int fd = ::open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0644);
  if (fd < 0) {
    return denied(errno);
  }
  ::close(fd);
  return "ok";
}

std::string try_spawn() {
  pid_t pid = 0;
  std::string sh = "/bin/sh";
  std::string flag = "-c";
  std::string script = "true";
  char* argv[] = {sh.data(), flag.data(), script.data(), nullptr};  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  const int rc = ::posix_spawn(&pid, sh.c_str(), nullptr, nullptr, argv, environ);
  if (rc != 0) {
    return denied(rc);
  }
  int status = 0;
  ::waitpid(pid, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? "ok" : "denied (child failed)";
}

// Applies one load-time directive.
void load_directive(hefesto_engine* e, std::string_view directive) {
  const std::size_t space = directive.find(' ');
  const std::string_view word = directive.substr(0, space);
  const std::string arg = space == std::string_view::npos ? std::string{} : std::string{directive.substr(space + 1)};
  if (word == "sleep-load") {
    std::this_thread::sleep_for(std::chrono::milliseconds(std::atoi(arg.c_str())));
  } else if (word == "net") {
    say(e, "net: " + try_connect(std::atoi(arg.c_str())));
  } else if (word == "write") {
    say(e, "write: " + try_write(arg));
  } else if (word == "spawn") {
    say(e, "spawn: " + try_spawn());
  } else if (word == "stderr") {
    say(e, arg, true);
  } else if (word == "points") {
    e->points = static_cast<std::size_t>(std::atol(arg.c_str()));
  }
}

bool has_directive(const hefesto_engine* e, std::string_view word) {
  for (const std::string& line : e->deck) {
    if (std::string_view{line}.starts_with(kDirective) && std::string_view{line}.substr(kDirective.size()) == word) {
      return true;
    }
  }
  return false;
}

void join(hefesto_engine* e) {
  if (e->runner.joinable()) {
    e->runner.join();
  }
}

void compute(hefesto_engine* e) {
  std::uint64_t deck_hash = 1469598103934665603ULL;
  for (const std::string& line : e->deck) {
    for (const char c : line) {
      deck_hash = (deck_hash ^ static_cast<unsigned char>(c)) * 1099511628211ULL;
    }
  }
  ++e->plots;
  e->time.assign(e->points, 0.0);
  e->out.assign(e->points, 0.0);
  for (std::size_t i = 0; i < e->points; ++i) {
    e->time[i] = static_cast<double>(i) * 1e-9;
    e->out[i] = next(e->random) + static_cast<double>(deck_hash % 1000U) +
                1000.0 * static_cast<double>(e->circuits) + 1e6 * static_cast<double>(e->plots);
  }
}

// --- hefesto_engine_v1 ---------------------------------------------------------

hefesto_engine* api_create(const hefesto_engine_callbacks* callbacks) {
  if (g_created.exchange(true)) {
    return nullptr;
  }
  auto* e = new hefesto_engine{};  // NOLINT(cppcoreguidelines-owning-memory)
  e->callbacks = *callbacks;
  e->random = 0x5EED;  // a process-independent but wrong seed: setseed must replace it
  return e;
}

void api_destroy(hefesto_engine* e) {
  join(e);
  delete e;  // NOLINT(cppcoreguidelines-owning-memory)
}

int api_command(hefesto_engine* e, const char* line) {
  const std::string_view text{line};
  say(e, "cmd: " + std::string{text});
  if (text.starts_with("setseed ")) {
    e->random = std::strtoull(line + 8, nullptr, 10);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }
  return HEFESTO_ENGINE_OK;
}

int api_load(hefesto_engine* e, const char* const* lines, std::size_t count) {
  join(e);
  e->deck.assign(lines, lines + count);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  e->points = 8;
  ++e->circuits;
  for (const std::string& line : e->deck) {
    if (std::string_view{line}.starts_with(kDirective)) {
      load_directive(e, std::string_view{line}.substr(kDirective.size()));
    }
  }
  say(e, "loaded " + std::to_string(count) + " lines");
  return HEFESTO_ENGINE_OK;
}

int api_run(hefesto_engine* e) {
  join(e);
  if (has_directive(e, "crash")) {
    std::abort();
  }
  if (has_directive(e, "stop")) {
    std::raise(SIGSTOP);
  }
  if (has_directive(e, "exit")) {
    say(e, "Error: controlled exit requested by the deck", true);
    state(e, HEFESTO_ENGINE_FAILED);
    return HEFESTO_ENGINE_OK;
  }
  // Like ngspice's bg_run: the analysis runs on another thread and the
  // state callbacks come from it.
  e->runner = std::thread([e] {
    state(e, HEFESTO_ENGINE_RUNNING);
    if (e->callbacks.on_progress != nullptr) {
      e->callbacks.on_progress(e->callbacks.user, "tran", 4, 50.0);
    }
    compute(e);
    state(e, HEFESTO_ENGINE_LOADED);
  });
  return HEFESTO_ENGINE_OK;
}

int api_ok(hefesto_engine* /*e*/) { return HEFESTO_ENGINE_OK; }

int api_clear(hefesto_engine* e) {
  join(e);
  say(e, "cmd: remcirc");
  say(e, "cmd: destroy all");
  e->circuits = 0;
  e->plots = 0;
  state(e, HEFESTO_ENGINE_IDLE);
  return HEFESTO_ENGINE_OK;
}

int api_vectors(hefesto_engine* e, void* user, hefesto_engine_vector_sink sink) {
  join(e);
  const hefesto_engine_vector times{"time", "time", 0, e->time.size(), e->time.data(), nullptr};
  const hefesto_engine_vector out{"v(out)", "voltage", 0, e->out.size(), e->out.data(), nullptr};
  sink(user, &times);
  sink(user, &out);
  return HEFESTO_ENGINE_OK;
}

const char* api_last_error(hefesto_engine* e) { return e->error.c_str(); }

const hefesto_engine_api kApi = {
    HEFESTO_ENGINE_ABI_VERSION, "fake 1", api_create, api_destroy, api_command, api_load, api_run, api_ok,
    api_ok,                     api_ok,   api_clear,  api_vectors, api_last_error,
};

}  // namespace

extern "C" __attribute__((visibility("default"))) const hefesto_engine_api* hefesto_engine_v1(void) {
  return &kApi;
}
