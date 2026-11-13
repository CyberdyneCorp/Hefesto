// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (openspec/changes/m5-mask-gds-cif/design.md D3): every
// operation fails with CORE-NOTIMPL-005 until INPUT ports the read engine
// (tasks.md section 7). Owner: INPUT.

#include "hefesto/mask/input/engine.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::mask {

struct ReadEngine::State {};

ReadEngine::ReadEngine(editor::EditorState& editor, const InputStyle& style, const compat::CompatSettings& compat)
    : editor_(editor), style_(style), compat_(compat), state_(std::make_unique<State>()) {}

ReadEngine::~ReadEngine() = default;

diag::Status ReadEngine::begin_cell(std::string_view /*name*/) {
  return cmd::not_implemented_status("mask::ReadEngine::begin_cell");
}

diag::Status ReadEngine::paint(std::size_t /*cif_layer*/, const ReadShape& /*shape*/) {
  return cmd::not_implemented_status("mask::ReadEngine::paint");
}

diag::Status ReadEngine::label(const ReadLabel& /*label*/) {
  return cmd::not_implemented_status("mask::ReadEngine::label");
}

diag::Status ReadEngine::call(std::string_view /*child*/, const db::Transform& /*transform*/,
                              const db::ArrayInfo& /*array*/) {
  return cmd::not_implemented_status("mask::ReadEngine::call");
}

diag::Status ReadEngine::property(std::string_view /*key*/, std::string_view /*value*/) {
  return cmd::not_implemented_status("mask::ReadEngine::property");
}

diag::Status ReadEngine::end_cell() {
  return cmd::not_implemented_status("mask::ReadEngine::end_cell");
}

diag::Status ReadEngine::finish() {
  // The stub keeps the members referenced: GCC 11 ignores [[maybe_unused]] on members (-Werror=attributes)
  // and Clang flags an unread private field. INPUT drops this with the real engine.
  static_cast<void>(editor_);
  static_cast<void>(style_);
  static_cast<void>(compat_);
  return cmd::not_implemented_status("mask::ReadEngine::finish");
}

}  // namespace hefesto::mask
