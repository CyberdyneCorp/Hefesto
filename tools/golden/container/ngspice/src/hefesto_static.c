/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The Hefesto Authors
 *
 * Static OSDI registry of libngspice-hefesto (Hefesto patch 0001): built-in
 * modules are found by the basename of the file a deck or an init file names;
 * other files are dlopen'ed only while dynamic loading is allowed
 * (simulation-engine-embedding, "OSDI compact models").
 */
#include "ngspice/ngspice.h"
#include "ngspice/cpextern.h"

#include "hefesto_static.h"

#include <dlfcn.h>
#include <string.h>

int hefesto_module_name(const char *path, const char *extension, char *out, size_t size)
{
    const char *base = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    size_t len = strlen(base);
    size_t ext = strlen(extension);
    if (len > ext && strcmp(base + len - ext, extension) == 0)
        len -= ext;
    if (len == 0 || len >= size)
        return 0;
    memcpy(out, base, len);
    out[len] = '\0';
    return 1;
}

static const HefestoOsdiModule *find_module(const char *name)
{
    for (size_t i = 0; i < hefesto_osdi_module_count; i++) {
        if (strcmp(hefesto_osdi_modules[i].name, name) == 0)
            return &hefesto_osdi_modules[i];
    }
    return NULL;
}

static const HefestoOsdiModule *as_module(const void *handle)
{
    const HefestoOsdiModule *first = hefesto_osdi_modules;
    const HefestoOsdiModule *end = first + hefesto_osdi_module_count;
    for (const HefestoOsdiModule *m = first; m < end; m++) {
        if ((const void *) m == handle)
            return m;
    }
    return NULL;
}

void *hefesto_osdi_builtin(const char *path)
{
    char name[256];
    if (!hefesto_module_name(path, ".osdi", name, sizeof name))
        return NULL;
    return (void *) find_module(name);
}

void *hefesto_osdi_sym(void *handle, const char *name)
{
    const HefestoOsdiModule *module = as_module(handle);
    if (!module)
        return dlsym(handle, name);
    for (size_t i = 0; i < module->symbol_count; i++) {
        if (strcmp(module->symbols[i].name, name) == 0)
            return module->symbols[i].address;
    }
    return NULL;
}

int hefesto_osdi_dynamic_allowed(void)
{
    return !cp_getvar("hefesto_osdi_static_only", CP_BOOL, NULL, 0);
}

void hefesto_osdi_report_not_builtin(const char *path)
{
    char name[256];
    if (!hefesto_module_name(path, ".osdi", name, sizeof name))
        snprintf(name, sizeof name, "%s", path);
    fprintf(stderr, "Error: OSDI model '%s' is not built into this app\n", name);
}
