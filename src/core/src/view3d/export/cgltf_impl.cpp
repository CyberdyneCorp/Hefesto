// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The one translation unit that compiles cgltf (v1.15, MIT; pinned in
// cmake/Dependencies.cmake): the reader (cgltf_parse, cgltf_validate, used
// by the export tests) and the writer (cgltf_write, gltf.cpp). Compiled
// with the third-party warnings silenced (sources.cmake).
//
// Owner: EXPORT (design D3, D14).

// cgltf_write.h includes cgltf.h again, so the reader's implementation
// macro must be gone by then.
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#undef CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#include <cgltf_write.h>
