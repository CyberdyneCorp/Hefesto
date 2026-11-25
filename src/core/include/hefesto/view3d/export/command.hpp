// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `export3d <file> [-format gltf|glb|stl] [-merged] [-zscale k]
// [-visible|-all]` (a Hefesto command, not Magic; hefesto-3d-visualization
// "3D export"; openspec/changes/m10-3d-view/design.md D14). It exports the
// open 3D window's current model, built as the window draws it (the
// client's scene: its stack, its cut box), with the window's visibility
// (`-visible`, the default) or every layer (`-all`).
//
// - Format: the file's extension (`.gltf`, `.glb`, `.stl`, any case) unless
//   `-format` names one. `.gltf` writes the JSON to <file> and the buffer to
//   <stem>.bin beside it; `.glb` one file.
// - STL: one file per exported layer, `<dir>/<stem>_<layer>.stl` (characters
//   other than letters, digits and `_-.+` in the layer name become `_`), or
//   with `-merged` one file, <file> as given.
// - `-zscale k` (k > 0) multiplies heights; k is recorded in the glTF.
// - The result is the list of files written. Every file is replaced
//   atomically.
// - No 3D window: VIEW3D-EXPORT-001; bad arguments: VIEW3D-EXPORT-002; a
//   window whose cell is gone: VIEW3D-EXPORT-004. Nothing is written then.
//
// The desktop's synthetic substrate slab is a picture only; it is never
// exported.
//
// Owner: EXPORT (design D3, task 11.3).

#pragma once

namespace hefesto::cmd {
class CommandRegistry;
}  // namespace hefesto::cmd

namespace hefesto::view3d {

/// Adds `export3d` to the default (layout) client table.
bool register_export3d_command(cmd::CommandRegistry& registry);

}  // namespace hefesto::view3d
