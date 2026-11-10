// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Test fixture of the ui area: a headless session with the built-ins, the
// ui commands and small fake `box`, `element`, `view`, `load`, `paint`,
// `select`, `what`, `cellname` commands that record their calls, so the
// dispatch and script-helper logic is tested on its own.

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/cmd/builtins.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/ui/commands.hpp"
#include "hefesto/ui/diag_codes.hpp"
#include "hefesto/ui/line.hpp"

namespace hefesto::test {

class UiSession {
 public:
  explicit UiSession(bool fakes = true)
      : services_(platform::PortableOptions{.storage_root = "/app", .pdk_roots = {}, .in_memory = true,
                                            .inline_tasks = true}) {
    (void)cmd::register_builtins(registry);
    (void)ui::register_ui_commands(registry);
    if (fakes) {
      add_fakes();
    }
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services_.view(), project, script);
    dispatcher->set_editor(&editor);
  }

  /// Runs a command line (Magic splitting) with `origin`; the last result.
  cmd::CommandResult run(std::string_view line, cmd::Origin origin = cmd::Origin::script) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : ui::run_command_line(*dispatcher, line, origin)) {
      last = std::move(r);
    }
    return last;
  }

  /// The text the command printed (its UI-REPORT-001 / info diagnostics).
  static std::string printed(const cmd::CommandResult& r) {
    std::string out;
    for (const auto& d : r.diagnostics) {
      if (d.severity == diag::Severity::info) {
        out += (out.empty() ? "" : "\n") + d.text;
      }
    }
    return out;
  }
  static std::string first_error(const cmd::CommandResult& r) {
    for (const auto& d : r.diagnostics) {
      if (d.severity == diag::Severity::error) {
        return d.code + ": " + d.text;
      }
    }
    return {};
  }

  /// The recorded fake-command calls as lines ("box values 0 0 1 1").
  std::vector<std::string> calls_of(std::string_view name) const {
    std::vector<std::string> out;
    for (const auto& line : calls) {
      if (line.rfind(std::string{name} + " ", 0) == 0 || line == name) {
        out.push_back(line);
      }
    }
    return out;
  }

  platform::FileSystem& files() { return services_.view().files; }

  settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  std::vector<std::string> calls;
  std::vector<std::string> inbox;           // `element inbox` result
  std::string view_rect = "0 0 1000 1000";  // `view get` result
  std::string window_cell = "top";          // `cellname list window`
  std::string self_cell;                    // `cellname list self`
  std::string what_list = "{} {} {}";       // `what -list`
  /// Units scale of the fake box when units are not internal (internal
  /// units per reported unit), e.g. 100 for microns at 0.01 um.
  std::int64_t box_scale = 1;

 private:
  void record(const cmd::Invocation& inv) {
    std::string line = inv.name;
    for (const auto& a : inv.args) {
      line += " " + a;
    }
    calls.push_back(line);
  }

  static std::int64_t num(const std::string& s) { return std::stoll(s); }

  cmd::CommandResult fake_box(const cmd::Invocation& inv) {
    record(inv);
    const auto& a = inv.args;
    const bool internal = editor.units.unit == cmd::InputUnit::internal;
    const std::int64_t k = internal ? 1 : box_scale;
    if (a.size() == 1 && a[0] == "values") {
      if (!editor.box) {
        return cmd::CommandResult::failure(diag::make(ui::codes::kUsage, "no box"));
      }
      const db::Rect& b = *editor.box;
      return cmd::CommandResult::success(cmd::Value::List{std::int64_t{b.x0} / k, std::int64_t{b.y0} / k,
                                                          std::int64_t{b.x1} / k, std::int64_t{b.y1} / k});
    }
    if (a.size() == 5 && a[0] == "values") {
      editor.box = db::Rect{static_cast<db::Coord>(num(a[1]) * k), static_cast<db::Coord>(num(a[2]) * k),
                            static_cast<db::Coord>(num(a[3]) * k), static_cast<db::Coord>(num(a[4]) * k)};
    } else if (a.size() == 3 && a[0] == "size" && editor.box) {
      editor.box->x1 = editor.box->x0 + static_cast<db::Coord>(num(a[1]) * k);
      editor.box->y1 = editor.box->y0 + static_cast<db::Coord>(num(a[2]) * k);
    } else if (a.size() == 3 && a[0] == "position" && editor.box) {
      const db::Coord w = editor.box->x1 - editor.box->x0;
      const db::Coord h = editor.box->y1 - editor.box->y0;
      editor.box = db::Rect{static_cast<db::Coord>(num(a[1]) * k), static_cast<db::Coord>(num(a[2]) * k),
                            static_cast<db::Coord>(num(a[1]) * k) + w, static_cast<db::Coord>(num(a[2]) * k) + h};
    } else if (a.size() == 3 && a[0] == "move" && a[1] == "e" && editor.box) {
      const auto d = static_cast<db::Coord>(num(a[2]) * k);
      editor.box->x0 += d;
      editor.box->x1 += d;
    }
    return cmd::CommandResult::success();
  }

  cmd::CommandResult fake_simple(const cmd::Invocation& inv) {
    record(inv);
    const auto& a = inv.args;
    if (inv.name == "element" && a.size() == 1 && a[0] == "inbox") {
      cmd::Value::List names;
      for (const auto& n : inbox) {
        names.emplace_back(n);
      }
      return cmd::CommandResult::success(std::move(names));
    }
    if (inv.name == "view" && a.size() == 1 && a[0] == "get") {
      return cmd::CommandResult::success(view_rect);
    }
    if (inv.name == "cellname" && a.size() == 2) {
      return cmd::CommandResult::success(a[1] == "window" ? window_cell : self_cell);
    }
    if (inv.name == "what") {
      return cmd::CommandResult::success(what_list);
    }
    return cmd::CommandResult::success();
  }

  void add_fakes() {
    (void)registry.add({"box", "box ?args?", "fake", [this](cmd::CommandContext&, const cmd::Invocation& inv) {
                          return fake_box(inv);
                        }});
    for (const char* name : {"element", "view", "load", "paint", "select", "what", "cellname", "getcell", "zoom",
                             "copy", "wire", "redo", "undo"}) {
      (void)registry.add({name, std::string{name} + " ?args?", "fake",
                          [this](cmd::CommandContext&, const cmd::Invocation& inv) { return fake_simple(inv); }});
    }
  }

  platform::PortableServices services_;
};

}  // namespace hefesto::test
