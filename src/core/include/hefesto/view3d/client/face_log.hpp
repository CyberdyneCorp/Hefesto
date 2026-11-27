// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The 3D window's face log (m10-3d-view task 12.4, design D13): the
// polygons the open 3D window draws, in the format of Magic's GL capture
// shim (tools/golden/gl_capture/gl_capture.c), so that
// tools/golden/gl_capture/faces.py reduces Hefesto's mesh and Magic's GL
// calls to the same form. hefesto-cli writes it when HEFESTO_GL_CAPTURE
// names a file.
//
//     gl-capture 1
//     poly <style long name> <n> <x1> <y1> <z1> ... <xn> <yn> <zn>
//
// One `poly` line per face of the scene's mesh, coordinates in root units
// (times the P1 xy factor) printed with %.9g, z top = -h * s_z and z bottom
// = z top - t * s_z in float arithmetic, as W3D computes them
// (w3dRenderVolume, W3Dmain.c:406-412). Top faces are counter-clockwise
// seen from above, bottom faces clockwise, side faces counter-clockwise
// seen from the side they face (w3dFillTile, w3dFillPolygon,
// w3dFillDiagonal). Hidden layers are left out, and so are layers whose
// display style is neither solid nor stippled (N3, approved 2026-11-22,
// both modes: W3Dmain.c:406, :439 draw only those, as the renderer does).
// A style with no long name, or one with white space, is `#<index>`.
//
// Owner: WIND3D (design D3).

#pragma once

#include <string>
#include <utility>

#include "hefesto/diag/sink.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/view3d/client/scene.hpp"
#include "hefesto/view3d/client/session.hpp"

namespace hefesto::view3d {

/// The face log of `scene` drawn with `view`'s s_z; `styles` names the
/// styles and decides which are drawn (nullptr: every layer is drawn and
/// named `#<style index>`).
[[nodiscard]] std::string face_log(const Scene& scene, const ViewState& view, const display::StyleSet* styles);

/// A WindowDisplay that rewrites the face log on every redraw (so the file
/// holds the last frame, as the shim's does) and forwards each call to
/// `next` (may be null). Write errors are ignored: the log is a test aid.
/// Each redraw is a build: with magic-compat ON a mask-layer build reports
/// its generation errors to `errors` (Q-MISC-89; hefesto-cli passes a sink
/// that reports through its dispatcher, the console); null reports none.
class FaceLogDisplay final : public WindowDisplay {
 public:
  FaceLogDisplay(platform::FileSystem& files, platform::Path path, WindowDisplay* next = nullptr,
                 diag::DiagnosticSink* errors = nullptr)
      : files_(files), path_(std::move(path)), next_(next), errors_(errors) {}

  void redraw(const editor::EditorState& editor, const WindowState& window) override;
  void closed(const WindowState& window) override;

 private:
  platform::FileSystem& files_;
  platform::Path path_;
  WindowDisplay* next_;
  diag::DiagnosticSink* errors_;
};

}  // namespace hefesto::view3d
