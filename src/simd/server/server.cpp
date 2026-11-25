// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Owner: SIMD.

#include "server/server.hpp"

#include <unistd.h>

#include <algorithm>
#include <iterator>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>

namespace hefesto::simd {

namespace frames = sim::frames;

namespace {

struct VectorSink {
  FrameWriter& out;
  std::uint32_t count = 0;
};

// Sends one vector as chunks of at most kVectorChunkValues values.
void send_vector(void* user, const hefesto_engine_vector* vector) {
  auto& sink = *static_cast<VectorSink*>(user);
  const bool complex = vector->complex != 0 && vector->imag != nullptr;
  std::size_t offset = 0;
  do {
    const std::size_t count = std::min(frames::kVectorChunkValues, vector->length - offset);
    frames::VecChunk chunk;
    chunk.name = vector->name != nullptr ? vector->name : "";
    chunk.type = vector->type != nullptr ? vector->type : "";
    chunk.complex = complex;
    chunk.length = vector->length;
    chunk.offset = offset;
    chunk.real.assign(vector->real + offset, vector->real + offset + count);
    if (complex) {
      chunk.imag.assign(vector->imag + offset, vector->imag + offset + count);
    }
    sink.out.write(chunk);
    offset += count;
  } while (offset < vector->length);
  ++sink.count;
}

frames::State to_state(int state) {
  return state >= HEFESTO_ENGINE_IDLE && state <= HEFESTO_ENGINE_FAILED ? static_cast<frames::State>(state)
                                                                         : frames::State::failed;
}

}  // namespace

Server::Server(const hefesto_engine_api& api, FrameWriter& out, std::chrono::milliseconds heartbeat)
    : api_(api), out_(out), heartbeat_(heartbeat) {}

Server::~Server() {
  if (engine_ != nullptr) {
    api_.destroy(engine_);
  }
}

// --- adapter callbacks (any thread) ------------------------------------------------

void Server::on_output(void* user, const char* text, std::size_t size, int is_error) {
  auto* server = static_cast<Server*>(user);
  server->out_.write(frames::Output{is_error != 0 ? frames::Stream::error : frames::Stream::output,
                                    std::string{text != nullptr ? std::string_view{text, size} : std::string_view{}}});
}

void Server::on_progress(void* user, const char* status, std::size_t size, double point) {
  auto* server = static_cast<Server*>(user);
  server->out_.write(
      frames::Progress{std::string{status != nullptr ? std::string_view{status, size} : std::string_view{}}, point});
}

void Server::on_state(void* user, int state) {
  auto* server = static_cast<Server*>(user);
  switch (state) {
    case HEFESTO_ENGINE_LOADED:
    case HEFESTO_ENGINE_PAUSED:
      server->post([server, state] { server->finish_run(state); });
      break;
    case HEFESTO_ENGINE_FAILED:
      server->post([server] { server->fault(); });
      break;
    default:
      server->out_.write(frames::StateChange{to_state(state)});
      break;
  }
}

// --- threads ---------------------------------------------------------------------------

void Server::post(Task task) {
  {
    std::lock_guard lock{mutex_};
    tasks_.push_back(std::move(task));
  }
  wake_.notify_all();
}

void Server::engine_loop() {
  for (;;) {
    Task task;
    {
      std::unique_lock lock{mutex_};
      wake_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
      if (tasks_.empty()) {
        return;  // stopping, and every queued request was served
      }
      task = std::move(tasks_.front());
      tasks_.pop_front();
    }
    task();
  }
}

void Server::heartbeat_loop() {
  std::uint64_t sequence = 0;
  std::unique_lock lock{mutex_};
  while (!wake_.wait_for(lock, heartbeat_, [this] { return stopping_; })) {
    lock.unlock();
    out_.write(frames::Beat{++sequence});
    lock.lock();
  }
}

int Server::run(int in_fd, const std::vector<std::string>& notes) {
  const hefesto_engine_callbacks callbacks{this, on_output, on_progress, on_state};
  engine_ = api_.create(&callbacks);
  if (engine_ == nullptr) {
    out_.write(frames::Output{frames::Stream::error, "the simulation engine could not be initialised"});
    return kExitStartup;
  }
  out_.write(frames::Hello{frames::kProtocolVersion, api_.engine_version != nullptr ? api_.engine_version : ""});
  for (const std::string& note : notes) {
    out_.write(frames::Output{frames::Stream::output, note});
  }
  std::thread engine_thread{[this] { engine_loop(); }};
  std::thread heartbeat_thread{[this] { heartbeat_loop(); }};

  int status = kExitOk;
  std::vector<std::byte> body;
  std::string detail;
  for (;;) {
    const ReadResult result = read_frame(in_fd, body, detail);
    if (result == ReadResult::error) {
      out_.write(frames::Output{frames::Stream::error, "hefesto-simd: " + detail});
      status = kExitProtocol;
      break;
    }
    if (result == ReadResult::end || !dispatch(body)) {
      break;  // end of input, or quit
    }
  }
  {
    std::lock_guard lock{mutex_};
    stopping_ = true;
  }
  wake_.notify_all();
  engine_thread.join();
  heartbeat_thread.join();
  return status;
}

// False ends the server (quit); a protocol error ends the process.
bool Server::dispatch(const std::vector<std::byte>& body) {
  frames::Decoded decoded = frames::decode(body);
  if (decoded.error != frames::Error::none) {
    out_.write(frames::Output{frames::Stream::error, "hefesto-simd: " + decoded.detail});
    _exit(kExitProtocol);
  }
  if (auto* load = std::get_if<frames::Load>(&*decoded.message)) {
    post([this, chunk = std::move(*load)]() mutable { handle_load(std::move(chunk)); });
    return true;
  }
  const auto* cmd = std::get_if<frames::Cmd>(&*decoded.message);
  if (cmd == nullptr) {
    out_.write(frames::Output{frames::Stream::error, "hefesto-simd: unexpected frame from the core"});
    _exit(kExitProtocol);
  }
  if (cmd->command == frames::Command::quit) {
    post([this] { reply(static_cast<std::uint8_t>(frames::Command::quit), HEFESTO_ENGINE_OK); });
    return false;
  }
  post([this, command = cmd->command] { handle_command(command); });
  return true;
}

// --- engine thread -----------------------------------------------------------------------

void Server::clear_if_loaded() {
  if (loaded_) {
    const int status = api_.clear(engine_);
    if (status == HEFESTO_ENGINE_FAULT) {
      fault();
    }
    loaded_ = false;
  }
}

void Server::handle_load(frames::Load chunk) {
  if ((chunk.flags & frames::kLoadFirst) != 0) {
    clear_if_loaded();
    chunk.init_commands.push_back("setseed " + std::to_string(chunk.seed));
    for (const std::string& line : chunk.init_commands) {
      const int status = api_.command(engine_, line.c_str());
      if (status != HEFESTO_ENGINE_OK) {
        loading_ = false;
        reply(frames::kRequestLoad, status);
        return;
      }
    }
    pending_lines_.clear();
    loading_ = true;
  }
  if (!loading_) {
    return;  // the rest of a deck whose first part failed (its reply was sent)
  }
  std::move(chunk.lines.begin(), chunk.lines.end(), std::back_inserter(pending_lines_));
  if ((chunk.flags & frames::kLoadLast) == 0) {
    return;
  }
  std::vector<const char*> lines;
  lines.reserve(pending_lines_.size());
  for (const std::string& line : pending_lines_) {
    lines.push_back(line.c_str());
  }
  const int status = api_.load(engine_, lines.data(), lines.size());
  loading_ = false;
  loaded_ = true;  // even a failed load may leave a circuit behind: clear it next time
  pending_lines_.clear();
  reply(frames::kRequestLoad, status);
}

void Server::handle_command(frames::Command command) {
  int status = HEFESTO_ENGINE_ERROR;
  switch (command) {
    case frames::Command::run: status = api_.run(engine_); break;
    case frames::Command::pause: status = api_.pause(engine_); break;
    case frames::Command::resume: status = api_.resume(engine_); break;
    case frames::Command::cancel: status = api_.cancel(engine_); break;
    case frames::Command::reset:
      loaded_ = true;
      clear_if_loaded();
      status = HEFESTO_ENGINE_OK;
      break;
    case frames::Command::quit: status = HEFESTO_ENGINE_OK; break;
  }
  reply(static_cast<std::uint8_t>(command), status);
}

void Server::reply(std::uint8_t request, int status) {
  const bool ok = status == HEFESTO_ENGINE_OK;
  std::string message;
  if (!ok) {
    const char* error = api_.last_error(engine_);
    message = error != nullptr && *error != '\0' ? error : (status == HEFESTO_ENGINE_BUSY ? "engine busy" : "failed");
  }
  out_.write(frames::Reply{request, ok, std::move(message)});
  if (status == HEFESTO_ENGINE_FAULT) {
    fault();
  }
}

void Server::finish_run(int state) {
  send_vectors();
  out_.write(frames::StateChange{to_state(state)});
}

void Server::send_vectors() {
  VectorSink sink{out_};
  if (api_.vectors(engine_, &sink, send_vector) == HEFESTO_ENGINE_FAULT) {
    fault();
  }
  out_.write(frames::VectorsDone{sink.count});
}

// A faulted engine is never reused (D-25 (a)): report and exit; the core
// replaces the worker.
void Server::fault() {
  out_.write(frames::StateChange{frames::State::failed});
  _exit(kExitFault);
}

}  // namespace hefesto::simd
