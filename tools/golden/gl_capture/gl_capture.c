/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The Hefesto Authors
 *
 * gl_capture: an LD_PRELOAD library that records the polygons Magic's 3D window draws
 * (openspec/changes/m10-3d-view design D13, task 12.4). Built into the magic-gl image
 * (tools/golden/container/Containerfile, WITH_OPENGL=1) as /opt/ref/lib/gl_capture.so and
 * preloaded into Magic by goldenlib/refrunners.py for a case with `[reference] gl_capture = true`.
 *
 * With HEFESTO_GL_CAPTURE naming a file, every glClear that clears the depth buffer (only
 * w3dClear, graphics/W3Dmain.c:465, does) starts a new frame: the file is truncated and gets the
 * header line "gl-capture 1". Each glBegin/glVertex3f.../glEnd group then appends one line
 *
 *     poly <style> <n> <x1> <y1> <z1> ... <xn> <yn> <zn>
 *
 * where <style> is the long name of Magic's current display style (GrStyleTable[grCurDStyle],
 * the style W3D set with GrSetStuff before drawing the tile) and the coordinates are the floats
 * passed to glVertex3f, printed with %.9g. The file therefore holds the last frame drawn. Only
 * glVertex3f is recorded: Magic's layout-window driver (grTOGL*.c) draws with glVertex2i and
 * the GLU tessellator's glVertex3dv, never glVertex3f, so those groups are empty and dropped.
 * tools/golden/gl_capture/faces.py reduces the file to the compared form.
 *
 * Magic's tclmagic.so is loaded by Tcl with RTLD_LOCAL, so its globals are looked up through
 * a handle reopened with RTLD_NOLOAD; the GL entry points are forwarded with RTLD_NEXT (this
 * library links libGL, which puts it in the global scope after us).
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef float GLfloat;

#define GL_DEPTH_BUFFER_BIT 0x00000100u
#define MAX_VERTICES 64

/* GR_STYLE_LINE of graphics/graphics.h at the pinned Magic commit fc5daa9e. */
typedef struct {
    int idx, mask, color, outline, fill, stipple;
    char shortname;
    char *longname;
} MagicStyle;

static FILE *capture_file;
static int in_group;
static int vertex_count;
static GLfloat vertices[MAX_VERTICES][3];
static char group_style[128];

static void *next_symbol(const char *name)
{
    void *sym = dlsym(RTLD_NEXT, name);
    if (sym == NULL)
        fprintf(stderr, "gl_capture: %s not found\n", name);
    return sym;
}

static int find_tclmagic(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)size;
    if (info->dlpi_name != NULL && strstr(info->dlpi_name, "tclmagic") != NULL) {
        *(void **)data = dlopen(info->dlpi_name, RTLD_NOW | RTLD_NOLOAD);
        return 1;
    }
    return 0;
}

static void *magic_symbol(const char *name)
{
    static void *handle;
    void *sym = dlsym(RTLD_DEFAULT, name);
    if (sym != NULL)
        return sym;
    if (handle == NULL)
        dl_iterate_phdr(find_tclmagic, &handle);
    return handle != NULL ? dlsym(handle, name) : NULL;
}

static void current_style(char *out, size_t size)
{
    int *style = magic_symbol("grCurDStyle");
    MagicStyle **table = magic_symbol("GrStyleTable");
    if (style == NULL || table == NULL || *table == NULL) {
        snprintf(out, size, "?");
        return;
    }
    const char *name = (*table)[*style].longname;
    if (name != NULL && name[0] != '\0' && strpbrk(name, " \t\n") == NULL)
        snprintf(out, size, "%s", name);
    else
        snprintf(out, size, "#%d", *style);
}

static void start_frame(void)
{
    const char *path = getenv("HEFESTO_GL_CAPTURE");
    if (path == NULL || path[0] == '\0')
        return;
    if (capture_file != NULL)
        fclose(capture_file);
    capture_file = fopen(path, "w");
    if (capture_file == NULL)
        return;
    fputs("gl-capture 1\n", capture_file);
    fflush(capture_file);
}

static void write_group(void)
{
    if (capture_file == NULL || vertex_count == 0)
        return;
    fprintf(capture_file, "poly %s %d", group_style, vertex_count);
    for (int i = 0; i < vertex_count; i++)
        fprintf(capture_file, " %.9g %.9g %.9g", vertices[i][0], vertices[i][1], vertices[i][2]);
    fputc('\n', capture_file);
    fflush(capture_file);
}

void glClear(GLbitfield mask)
{
    static void (*real)(GLbitfield);
    if (real == NULL)
        real = (void (*)(GLbitfield))next_symbol("glClear");
    if (mask & GL_DEPTH_BUFFER_BIT)
        start_frame();
    if (real != NULL)
        real(mask);
}

void glBegin(GLenum mode)
{
    static void (*real)(GLenum);
    if (real == NULL)
        real = (void (*)(GLenum))next_symbol("glBegin");
    in_group = 1;
    vertex_count = 0;
    current_style(group_style, sizeof group_style);
    if (real != NULL)
        real(mode);
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    static void (*real)(GLfloat, GLfloat, GLfloat);
    if (real == NULL)
        real = (void (*)(GLfloat, GLfloat, GLfloat))next_symbol("glVertex3f");
    if (in_group && vertex_count < MAX_VERTICES) {
        vertices[vertex_count][0] = x;
        vertices[vertex_count][1] = y;
        vertices[vertex_count][2] = z;
        vertex_count++;
    }
    if (real != NULL)
        real(x, y, z);
}

void glEnd(void)
{
    static void (*real)(void);
    if (real == NULL)
        real = (void (*)(void))next_symbol("glEnd");
    write_group();
    in_group = 0;
    vertex_count = 0;
    if (real != NULL)
        real();
}
