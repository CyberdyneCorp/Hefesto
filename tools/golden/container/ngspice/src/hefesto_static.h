/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The Hefesto Authors
 *
 * Built-in registries of libngspice-hefesto, called from the ngspice sources
 * by the Hefesto patches 0001 (static OSDI registry) and 0002 (static
 * code-model registration). See third_party/ngspice/patches/series.
 */
#ifndef HEFESTO_NGSPICE_STATIC_H
#define HEFESTO_NGSPICE_STATIC_H

#include <stddef.h>

/* ---- XSPICE code models (patch 0002; hefesto_cm_registry.c, generated) ---- */

/* Adds every built-in code model and user-defined node type to the device
 * tables, once. Called by spice_init_devices(). */
void hefesto_cm_register_all(void);

/* Non-zero when `path` names a built-in code-model library: its basename
 * without the ".cm" extension is one of the libraries compiled in. */
int hefesto_cm_is_builtin(const char *path);

/* ---- OSDI compact models (patch 0001; hefesto_static.c) ---- */

/* One exported symbol of a built-in OSDI module (OSDI_VERSION_MAJOR,
 * OSDI_DESCRIPTORS, osdi_log, ...), as the module's object file defines it. */
typedef struct {
    const char *name;
    void *address;
} HefestoOsdiSymbol;

/* A built-in OSDI module: `name` is the basename of the .osdi file a deck or
 * an init file names (for example "psp103_nqs"). */
typedef struct {
    const char *name;
    const HefestoOsdiSymbol *symbols;
    size_t symbol_count;
} HefestoOsdiModule;

/* The built-in modules (hefesto_osdi_builtins.c, generated). */
extern const HefestoOsdiModule hefesto_osdi_modules[];
extern const size_t hefesto_osdi_module_count;

/* The module whose name is the basename of `path` without ".osdi", or NULL.
 * The result is an opaque handle for hefesto_osdi_sym(). */
void *hefesto_osdi_builtin(const char *path);

/* hefesto_osdi_builtin() handles look up the module's table; any other
 * handle came from dlopen and is passed to dlsym. */
void *hefesto_osdi_sym(void *handle, const char *name);

/* Zero while the ngspice variable `hefesto_osdi_static_only` is set. */
int hefesto_osdi_dynamic_allowed(void);

/* Writes "Error: OSDI model '<name>' is not built into this app". */
void hefesto_osdi_report_not_builtin(const char *path);

/* The basename of `path` without `extension` (when it ends with it), copied
 * into `out` of `size` bytes; returns 0 when it does not fit. */
int hefesto_module_name(const char *path, const char *extension, char *out, size_t size);

#endif /* HEFESTO_NGSPICE_STATIC_H */
