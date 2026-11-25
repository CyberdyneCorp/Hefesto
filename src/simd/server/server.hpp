// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The IPC server of hefesto-simd (openspec/changes/m9-lvs-simulation design
// D9 "Worker", D10; capability simulation-engine-embedding "Crash
// containment on desktop", "Worker channel limits", "Engine threading
// model", "Single engine instance per address space").
//
// Three threads:
//   - the reader (the caller of run()) decodes frames from standard input
//     and queues load and cmd requests;
//   - the engine thread makes every call into the adapter, in order, and
//     after a run ends sends the vectors and then the new state;
//   - the heartbeat thread writes a beat every interval and never touches
//     the engine, so a long deck load or analysis cannot stop it.
//
// Reuse between jobs: `reset` (and a load that follows a loaded job without
// one) clears the circuit and every plot; the first load frame's init
// commands and `setseed <seed>` are sent before the deck.
//
// Exit statuses: 0 quit or end of input, 3 engine fault, 5 protocol error.
//
// Owner: SIMD.

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "abi/hefesto_engine_v1.h"
#include "ipc/frame_io.hpp"

namespace hefesto::simd {

inline constexpr int kExitOk = 0;
inline constexpr int kExitFault = 3;
inline constexpr int kExitStartup = 4;
inline constexpr int kExitProtocol = 5;

class Server {
 public:
  Server(const hefesto_engine_api& api, FrameWriter& out, std::chrono::milliseconds heartbeat);
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;
  ~Server();

  /// Creates the engine, says hello (with `notes` as job-log lines), serves
  /// frames from `in_fd` and returns the exit status.
  int run(int in_fd, const std::vector<std::string>& notes);

 private:
  using Task = std::function<void()>;

  static void on_output(void* user, const char* text, std::size_t size, int is_error);
  static void on_progress(void* user, const char* status, std::size_t size, double point);
  static void on_state(void* user, int state);

  void post(Task task);
  void engine_loop();
  void heartbeat_loop();
  bool dispatch(const std::vector<std::byte>& body);

  void handle_load(sim::frames::Load chunk);
  void handle_command(sim::frames::Command command);
  void finish_run(int state);
  void send_vectors();
  void reply(std::uint8_t request, int status);
  void fault();
  void clear_if_loaded();

  const hefesto_engine_api& api_;
  FrameWriter& out_;
  std::chrono::milliseconds heartbeat_;
  hefesto_engine* engine_ = nullptr;

  std::mutex mutex_;
  std::condition_variable wake_;
  std::deque<Task> tasks_;
  bool stopping_ = false;

  // Engine thread only.
  bool loaded_ = false;
  std::vector<std::string> pending_lines_;
  bool loading_ = false;
};

}  // namespace hefesto::simd
