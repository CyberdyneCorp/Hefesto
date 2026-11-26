// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The hefesto-ngspice-adapter plugin: hefesto_engine_v1 over ngspice's
// shared-library API (openspec/changes/m9-lvs-simulation design D2, D9;
// capability simulation-engine-embedding "Engine adapter boundary",
// "Deterministic engine initialisation", "Single engine instance per address
// space", "Engine threading model", "Callback contract", "Cancellation and
// fault states").
//
// This is the only Hefesto file that includes sharedspice.h
// (scripts/check_engine_boundary.py, rule 3). Every string and array that
// ngspice hands over is copied before the callback or call returns; nothing
// allocated by ngspice is freed here and ngspice frees nothing of ours.
//
// Owner: SIMD.

#include <ngspice/sharedspice.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "abi/hefesto_engine_v1.h"

// The engine instance (the C ABI's opaque type).
struct hefesto_engine {
  hefesto_engine_callbacks callbacks{};
  std::mutex mutex;
  std::condition_variable changed;
  bool background = false;      // ngspice's background thread runs
  bool halt_requested = false;  // the end of the run is a pause or a cancel
  bool faulted = false;         // ControlledExit was called
  std::deque<std::string> error_lines;  // the last 50 error-stream lines
  std::string last_error;
};

namespace {

constexpr std::size_t kKeptErrorLines = 50;
constexpr auto kHaltTimeout = std::chrono::seconds(2);
constexpr std::string_view kErrorPrefix = "stderr ";

std::atomic<bool> g_created{false};

// ngspice's type names in the order of its SV_* enumeration (frontend/typesdef.c).
constexpr std::array<const char*, 24> kTypeNames = {
    "notype",      "time",        "frequency",         "voltage",           "current",   "voltage-density",
    "current-density", "voltage^2-density", "current^2-density", "voltage^2", "current^2", "pole",
    "zero",        "s-param",     "temp-sweep",        "res-sweep",         "impedance", "admittance",
    "power",       "phase",       "decibel",           "capacitance",       "charge",    "temperature"};
constexpr short kComplexFlag = 1 << 1;  // VF_COMPLEX

const char* type_name(int type) {
  return type >= 0 && static_cast<std::size_t>(type) < kTypeNames.size() ? kTypeNames[static_cast<std::size_t>(type)]
                                                                         : "notype";
}

hefesto_engine* self(void* user) { return static_cast<hefesto_engine*>(user); }

// ngspice wants char*; it never keeps the pointer past the call.
int command(const std::string& line) {
  std::string copy = line;
  return ngSpice_Command(copy.data());
}

// --- ngspice callbacks (any thread; copy and return) ---------------------------

int ng_char(char* text, int /*id*/, void* user) {
  hefesto_engine* engine = self(user);
  std::string_view line = text != nullptr ? std::string_view{text} : std::string_view{};
  const bool is_error = line.starts_with(kErrorPrefix);
  if (is_error) {
    line.remove_prefix(kErrorPrefix.size());
    std::lock_guard lock{engine->mutex};
    engine->error_lines.emplace_back(line);
    if (engine->error_lines.size() > kKeptErrorLines) {
      engine->error_lines.pop_front();
    }
  }
  if (engine->callbacks.on_output != nullptr) {
    const std::string copy{line};
    engine->callbacks.on_output(engine->callbacks.user, copy.c_str(), copy.size(), is_error ? 1 : 0);
  }
  return 0;
}

// "tran: 34.5%" -> ("tran", 34.5); text without a percentage -> (text, -1).
int ng_stat(char* text, int /*id*/, void* user) {
  hefesto_engine* engine = self(user);
  const std::string copy = text != nullptr ? std::string{text} : std::string{};
  std::string status = copy;
  double point = -1.0;
  const std::size_t colon = copy.rfind(':');
  if (colon != std::string::npos && copy.find('%', colon) != std::string::npos) {
    status = copy.substr(0, colon);
    point = std::strtod(copy.c_str() + colon + 1, nullptr);
  }
  if (engine->callbacks.on_progress != nullptr) {
    engine->callbacks.on_progress(engine->callbacks.user, status.c_str(), status.size(), point);
  }
  return 0;
}

int ng_exit(int /*status*/, NG_BOOL /*immediate*/, NG_BOOL /*quit*/, int /*id*/, void* user) {
  hefesto_engine* engine = self(user);
  {
    std::lock_guard lock{engine->mutex};
    engine->faulted = true;
  }
  engine->changed.notify_all();
  if (engine->callbacks.on_state != nullptr) {
    engine->callbacks.on_state(engine->callbacks.user, HEFESTO_ENGINE_FAILED);
  }
  return 0;
}

// The live stream (ABI 2): one point, every vector's real part, the scale
// first. The names point into ngspice's own strings, valid for the call;
// hefesto-simd copies the points it keeps and decimates the rest. M9 R2
// review: the names were copied for every point, at the full simulation
// rate, although hefesto-simd keeps one point per 10 ms; the buffer is now
// reused per thread and nothing is copied here.
int ng_data(pvecvaluesall values, int /*count*/, int /*id*/, void* user) {
  hefesto_engine* engine = self(user);
  if (engine->callbacks.on_data == nullptr || values == nullptr || values->vecsa == nullptr || values->veccount <= 0) {
    return 0;
  }
  thread_local std::vector<hefesto_engine_value> point;
  point.clear();
  const auto count = static_cast<std::size_t>(values->veccount);
  for (std::size_t i = 0; i < count; ++i) {
    const pvecvalues v = values->vecsa[i];
    if (v == nullptr) {
      continue;
    }
    point.push_back(hefesto_engine_value{v->name != nullptr ? v->name : "", v->creal});
    if (v->is_scale && point.size() > 1) {
      std::swap(point.front(), point.back());
    }
  }
  engine->callbacks.on_data(engine->callbacks.user, point.data(), point.size());
  return 0;
}
int ng_init_data(pvecinfoall /*info*/, int /*id*/, void* /*user*/) { return 0; }

// ngspice passes `true` when the background thread has ended.
int ng_background(NG_BOOL ended, int /*id*/, void* user) {
  hefesto_engine* engine = self(user);
  bool halted = false;
  {
    std::lock_guard lock{engine->mutex};
    engine->background = !ended;
    halted = engine->halt_requested;
  }
  engine->changed.notify_all();
  if (engine->callbacks.on_state != nullptr) {
    const int state = !ended ? HEFESTO_ENGINE_RUNNING : (halted ? HEFESTO_ENGINE_PAUSED : HEFESTO_ENGINE_LOADED);
    engine->callbacks.on_state(engine->callbacks.user, state);
  }
  return 0;
}

// --- helpers on the engine thread -----------------------------------------------

int fail(hefesto_engine* engine, std::string message) {
  std::lock_guard lock{engine->mutex};
  for (const std::string& line : engine->error_lines) {
    message += "\n" + line;
  }
  engine->last_error = std::move(message);
  return engine->faulted ? HEFESTO_ENGINE_FAULT : HEFESTO_ENGINE_ERROR;
}

int busy(hefesto_engine* engine) {
  fail(engine, "engine busy");
  return HEFESTO_ENGINE_BUSY;
}

bool faulted(hefesto_engine* engine) {
  std::lock_guard lock{engine->mutex};
  return engine->faulted;
}

void clear_errors(hefesto_engine* engine) {
  std::lock_guard lock{engine->mutex};
  engine->error_lines.clear();
  engine->last_error.clear();
}

// Waits at most kHaltTimeout for the background thread to end.
bool wait_ended(hefesto_engine* engine) {
  std::unique_lock lock{engine->mutex};
  return engine->changed.wait_for(lock, kHaltTimeout, [engine] { return !engine->background || engine->faulted; }) &&
         !engine->faulted;
}

int checked_command(hefesto_engine* engine, const std::string& line) {
  if (faulted(engine)) {
    return fail(engine, "the engine is faulted");
  }
  if (ngSpice_running()) {
    return busy(engine);
  }
  return command(line) == 0 ? HEFESTO_ENGINE_OK : fail(engine, "command failed: " + line);
}

int halt(hefesto_engine* engine) {
  if (!ngSpice_running()) {
    return fail(engine, "no analysis is running");
  }
  {
    std::lock_guard lock{engine->mutex};
    engine->halt_requested = true;
  }
  command("bg_halt");
  if (!wait_ended(engine)) {
    {
      std::lock_guard lock{engine->mutex};
      engine->faulted = true;
    }
    if (engine->callbacks.on_state != nullptr) {
      engine->callbacks.on_state(engine->callbacks.user, HEFESTO_ENGINE_FAILED);
    }
    return fail(engine, "the analysis did not stop within 2 s");
  }
  return HEFESTO_ENGINE_OK;
}

// --- hefesto_engine_v1 ---------------------------------------------------------------

hefesto_engine* api_create(const hefesto_engine_callbacks* callbacks) {
  if (g_created.exchange(true)) {
    return nullptr;  // one instance per address space
  }
  auto* engine = new hefesto_engine{};  // NOLINT(cppcoreguidelines-owning-memory): owned by the C caller
  if (callbacks != nullptr) {
    engine->callbacks = *callbacks;
  }
  ngSpice_nospinit();
  ngSpice_nospiceinit();
  if (ngSpice_Init(ng_char, ng_stat, ng_exit, ng_data, ng_init_data, ng_background, engine) != 0) {
    delete engine;  // NOLINT(cppcoreguidelines-owning-memory)
    return nullptr;
  }
  for (const char* line : {"set noaskquit", "set nomoremode", "set filetype=binary"}) {
    command(line);
  }
  return engine;
}

void api_destroy(hefesto_engine* engine) {
  if (engine == nullptr) {
    return;
  }
  if (ngSpice_running()) {
    halt(engine);
  }
  // The process-wide ngspice state stays; a second create() in this process
  // is refused (g_created stays set).
  delete engine;  // NOLINT(cppcoreguidelines-owning-memory)
}

int api_command(hefesto_engine* engine, const char* line) {
  clear_errors(engine);
  return checked_command(engine, line != nullptr ? line : "");
}

int api_load(hefesto_engine* engine, const char* const* lines, std::size_t count) {
  clear_errors(engine);
  if (faulted(engine)) {
    return fail(engine, "the engine is faulted");
  }
  if (ngSpice_running()) {
    return busy(engine);
  }
  std::vector<std::string> copies(lines, lines + count);
  const bool has_end = !copies.empty() && copies.back().starts_with(".end") && copies.back().size() <= 4;
  if (!has_end) {
    copies.emplace_back(".end");
  }
  std::vector<char*> circ;
  circ.reserve(copies.size() + 1);
  for (std::string& line : copies) {
    circ.push_back(line.data());
  }
  circ.push_back(nullptr);
  {
    std::lock_guard lock{engine->mutex};
    engine->halt_requested = false;
  }
  return ngSpice_Circ(circ.data()) == 0 && !faulted(engine) ? HEFESTO_ENGINE_OK : fail(engine, "the deck was not loaded");
}

int api_run(hefesto_engine* engine) {
  clear_errors(engine);
  {
    std::lock_guard lock{engine->mutex};
    engine->halt_requested = false;
  }
  return checked_command(engine, "bg_run");
}

int api_pause(hefesto_engine* engine) {
  clear_errors(engine);
  return halt(engine);
}

int api_resume(hefesto_engine* engine) {
  clear_errors(engine);
  {
    std::lock_guard lock{engine->mutex};
    engine->halt_requested = false;
  }
  return checked_command(engine, "bg_resume");
}

int api_cancel(hefesto_engine* engine) {
  clear_errors(engine);
  return ngSpice_running() ? halt(engine) : HEFESTO_ENGINE_OK;
}

int api_clear(hefesto_engine* engine) {
  clear_errors(engine);
  if (faulted(engine)) {
    return fail(engine, "the engine is faulted");
  }
  if (ngSpice_running()) {
    return busy(engine);
  }
  command("remcirc");  // fails harmlessly when no circuit is loaded
  command("destroy all");
  if (engine->callbacks.on_state != nullptr) {
    engine->callbacks.on_state(engine->callbacks.user, HEFESTO_ENGINE_IDLE);
  }
  return HEFESTO_ENGINE_OK;
}

// One vector, copied out of ngspice.
struct VectorCopy {
  std::string name;
  std::string type;
  bool complex = false;
  std::vector<double> real;
  std::vector<double> imag;
};

// Copies the data of `info` (called while reallocation is locked).
void copy_data(const vector_info& info, VectorCopy& out) {
  const auto length = static_cast<std::size_t>(info.v_length);
  out.name = info.v_name != nullptr ? info.v_name : "";
  out.type = type_name(info.v_type);
  out.complex = (info.v_flags & kComplexFlag) != 0 && info.v_compdata != nullptr;
  if (out.complex) {
    out.real.resize(length);
    out.imag.resize(length);
    for (std::size_t i = 0; i < length; ++i) {
      out.real[i] = info.v_compdata[i].cx_real;
      out.imag[i] = info.v_compdata[i].cx_imag;
    }
  } else if (info.v_realdata != nullptr) {
    out.real.assign(info.v_realdata, info.v_realdata + length);
  } else {
    out.real.assign(length, 0.0);
  }
}

// Copies one vector while reallocation is locked (simulation-engine-embedding
// "Reading vectors during a run"): ngGet_Vec_Info's pointers are read only
// inside the lock.
bool copy_vector(const std::string& full_name, VectorCopy& out) {
  std::string query = full_name;
  ngSpice_LockRealloc();
  const pvector_info info = ngGet_Vec_Info(query.data());
  const bool found = info != nullptr && info->v_length >= 0;
  if (found) {
    copy_data(*info, out);
  }
  ngSpice_UnlockRealloc();
  return found;
}

// The current plot's vector names, copied ("const" has none of the job's).
std::vector<std::string> vector_names(std::string& plot) {
  const char* current = ngSpice_CurPlot();
  plot = current != nullptr ? current : "";
  std::vector<std::string> names;
  if (plot.empty() || plot == "const") {
    return names;
  }
  char** all = ngSpice_AllVecs(plot.data());
  for (std::size_t i = 0; all != nullptr && all[i] != nullptr; ++i) {
    names.emplace_back(all[i]);
  }
  return names;
}

int api_vectors(hefesto_engine* engine, void* user, hefesto_engine_vector_sink sink) {
  clear_errors(engine);
  if (faulted(engine)) {
    return fail(engine, "the engine is faulted");
  }
  std::string plot;
  for (const std::string& vector : vector_names(plot)) {
    VectorCopy copy;
    if (!copy_vector(plot + "." + vector, copy)) {
      continue;
    }
    const hefesto_engine_vector out{copy.name.c_str(), copy.type.c_str(), copy.complex ? 1 : 0, copy.real.size(),
                                    copy.real.data(),   copy.complex ? copy.imag.data() : nullptr};
    sink(user, &out);
  }
  return HEFESTO_ENGINE_OK;
}

const char* api_last_error(hefesto_engine* engine) {
  return engine != nullptr ? engine->last_error.c_str() : "";
}

const hefesto_engine_api kApi = {
    HEFESTO_ENGINE_ABI_VERSION,
    "ngspice " NGSPICE_HEFESTO_VERSION,
    api_create,
    api_destroy,
    api_command,
    api_load,
    api_run,
    api_pause,
    api_resume,
    api_cancel,
    api_clear,
    api_vectors,
    api_last_error,
};

}  // namespace

extern "C" __attribute__((visibility("default"))) const hefesto_engine_api* hefesto_engine_v1(void) {
  return &kApi;
}
