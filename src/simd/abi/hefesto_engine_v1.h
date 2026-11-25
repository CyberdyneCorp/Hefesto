/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The Hefesto Authors
 *
 * hefesto_engine_v1: the C ABI of the hefesto-ngspice-adapter plugin
 * (openspec/changes/m9-lvs-simulation design D2, Area contract SIMD;
 * capability simulation-engine-embedding "Engine adapter boundary").
 *
 * It mirrors sim::Engine and carries no ngspice type. The plugin exports one
 * symbol, `hefesto_engine_v1`, a function returning the table below. The
 * hefesto-simd worker (and, in developer builds only, the in-process
 * developer host) loads the plugin with dlopen and calls the table.
 *
 * Threading: the caller makes every call from one thread (the engine
 * thread). Callbacks can arrive on any thread (ngspice's background and
 * print threads); every pointer they get is valid only during the call, so
 * a callback copies what it keeps and returns quickly. Strings and arrays
 * passed to the plugin are copied before the call returns.
 *
 * Owner: SIMD.
 */
#ifndef HEFESTO_ENGINE_V1_H
#define HEFESTO_ENGINE_V1_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HEFESTO_ENGINE_ABI_VERSION 1u
#define HEFESTO_ENGINE_ENTRY "hefesto_engine_v1"

/* Engine states, in sim::EngineState's order. */
enum hefesto_engine_state {
  HEFESTO_ENGINE_IDLE = 0,
  HEFESTO_ENGINE_LOADED = 1,
  HEFESTO_ENGINE_RUNNING = 2,
  HEFESTO_ENGINE_PAUSED = 3,
  HEFESTO_ENGINE_FAILED = 4
};

/* Results of the calls. */
enum hefesto_engine_status {
  HEFESTO_ENGINE_OK = 0,
  HEFESTO_ENGINE_ERROR = 1,  /* the engine rejected the call; see last_error */
  HEFESTO_ENGINE_BUSY = 2,   /* the background analysis is running ("engine busy") */
  HEFESTO_ENGINE_FAULT = 3   /* the instance is faulted; it must be replaced */
};

typedef struct hefesto_engine hefesto_engine; /* opaque */

typedef struct hefesto_engine_callbacks {
  void* user;
  /* is_error: the line came on ngspice's error stream ("stderr " removed). */
  void (*on_output)(void* user, const char* text, size_t size, int is_error);
  /* point: percent done, or -1 when the status carries none. */
  void (*on_progress)(void* user, const char* status, size_t size, double point);
  void (*on_state)(void* user, int state);
} hefesto_engine_callbacks;

/* One final vector, valid only during the sink call. */
typedef struct hefesto_engine_vector {
  const char* name;
  const char* type; /* ngspice's type name: "voltage", "time", ... */
  int complex;
  size_t length;
  const double* real;
  const double* imag; /* NULL unless complex */
} hefesto_engine_vector;

typedef void (*hefesto_engine_vector_sink)(void* user, const hefesto_engine_vector* vector);

typedef struct hefesto_engine_api {
  uint32_t abi_version;          /* HEFESTO_ENGINE_ABI_VERSION */
  const char* engine_version;    /* e.g. "ngspice 47-hefesto1" */

  /* The one instance of this process (NULL when one exists already or the
   * engine cannot initialise). Initialisation is deterministic
   * (simulation-engine-embedding "Deterministic engine initialisation"). */
  hefesto_engine* (*create)(const hefesto_engine_callbacks* callbacks);
  void (*destroy)(hefesto_engine* engine);

  /* One engine command, e.g. a PDK initialisation command or "setseed 1".
   * HEFESTO_ENGINE_BUSY while the analysis runs. */
  int (*command)(hefesto_engine* engine, const char* line);
  /* Loads a deck (lines without line ends; ".end" is added when missing). */
  int (*load)(hefesto_engine* engine, const char* const* lines, size_t count);
  int (*run)(hefesto_engine* engine);
  int (*pause)(hefesto_engine* engine);
  int (*resume)(hefesto_engine* engine);
  /* Stops the analysis (at most 2 s); HEFESTO_ENGINE_FAULT when it does not stop. */
  int (*cancel)(hefesto_engine* engine);
  /* Clears the circuit and every plot ("remcirc", "destroy all"). */
  int (*clear)(hefesto_engine* engine);
  /* Calls `sink` once per vector of the current plot, in the plot's order. */
  int (*vectors)(hefesto_engine* engine, void* user, hefesto_engine_vector_sink sink);
  /* The message of the last failed call; valid until the next call. */
  const char* (*last_error)(hefesto_engine* engine);
} hefesto_engine_api;

typedef const hefesto_engine_api* (*hefesto_engine_entry_fn)(void);

#ifdef __cplusplus
}
#endif

#endif /* HEFESTO_ENGINE_V1_H */
