// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixtures of the lef/write unit tests: a command session over in-memory
// files with tests/data/lef/write/write.tech loaded (its lef section built
// from kLefLines plus each test's extra lines), cells painted and labelled
// in C++, and helpers that run `lef write` and read the output.
//
// Owner: LEFW (openspec/changes/m6-lef-def-oasis/design.md D2).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/lef/write/macro.hpp"
#include "hefesto/lef/write/write.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/ui/line.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::lef::write_test {

/// The tech `lef` section every test starts from.
inline constexpr std::string_view kLefLines = R"(
    routing metal1 met1
    routing metal2 met2
    contact pc licon
    cut m2c via1
    masterslice nwell nwell
    masterslice pwell pwell
    obstruction obsm1 met1
    obstruction obsm2 met2
    bound prboundary prb
)";

inline std::string read_text(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

inline std::string tech_text(std::string_view lef_lines) {
  std::string text = read_text(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "write.tech");
  text += "\nlef\n";
  text += lef_lines;
  text += "\nend\n";
  return text;
}

/// The session's files, refusing writes to paths that start with or equal
/// one of `refused`.
class FailingFiles final : public platform::FileSystem {
 public:
  FailingFiles(platform::FileSystem& base, std::vector<std::string> refused)
      : base_(base), refused_(std::move(refused)) {}

  diag::Outcome<std::string> read(const platform::Path& path) override { return base_.read(path); }
  diag::Status write_atomic(const platform::Path& path, std::string_view data) override {
    const std::string p = path.string();
    for (const std::string& r : refused_) {
      if (p == r || p.starts_with(r)) {
        return diag::make("PLATFORM-IO-002", "refused");
      }
    }
    return base_.write_atomic(path, data);
  }
  diag::Status append(const platform::Path& path, std::string_view data) override { return base_.append(path, data); }
  bool exists(const platform::Path& path) override { return base_.exists(path); }
  diag::Outcome<std::uint64_t> size(const platform::Path& path) override { return base_.size(path); }
  diag::Outcome<std::vector<platform::Path>> list(const platform::Path& d) override { return base_.list(d); }
  diag::Status create_directories(const platform::Path& d) override { return base_.create_directories(d); }
  diag::Status rename(const platform::Path& a, const platform::Path& b) override { return base_.rename(a, b); }
  diag::Status remove(const platform::Path& path) override { return base_.remove(path); }

 private:
  platform::FileSystem& base_;
  std::vector<std::string> refused_;
};

/// A label to add to a cell.
struct Port {
  std::string text;
  std::string type;
  db::Rect rect;
  std::optional<std::uint32_t> index;  // nullopt: not a port
  db::PortClass port_class = db::PortClass::default_class;
  db::PortUse use = db::PortUse::default_use;
  db::PortShape shape = db::PortShape::default_shape;
  bool sticky = false;
};

struct Session {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  diag::CollectingSink printed;
  std::vector<diag::Diagnostic> messages;

  explicit Session(bool magic_compat = true, std::string_view extra_lef = {}) {
    project.compat.magic_compat = magic_compat;
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    dispatcher->add_sink(printed);
    file("/t/write.tech", tech_text(std::string{kLefLines} + std::string{extra_lef}));
    REQUIRE(run("tech load /t/write.tech -noprompt").ok());
    (void)printed.take();
    messages.clear();
  }

  void file(const std::string& path, const std::string& text) {
    REQUIRE(services.view().files.write_atomic(path, text).ok());
  }

  [[nodiscard]] std::string read(const std::string& path) {
    auto text = services.view().files.read(path);
    REQUIRE(text.ok());
    return text.value();
  }

  [[nodiscard]] bool exists(const std::string& path) { return services.view().files.exists(path); }

  /// Runs a `;`-separated line; returns the first failure, else the last
  /// result. Every diagnostic is kept in `messages`.
  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : ui::run_command_line(*dispatcher, line, cmd::Origin::script)) {
      for (const auto& d : r.diagnostics) {
        messages.push_back(d);
      }
      if (!r.ok()) {
        return std::move(r);
      }
      last = std::move(r);
    }
    return last;
  }

  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = editor.technology->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  /// A new, available cell.
  db::CellDef& cell(std::string_view name) {
    auto made = editor.cells.create(name, editor.technology->plane_count());
    REQUIRE(made.ok());
    made.value()->flags().available = true;
    return *made.value();
  }

  void paint(db::CellDef& def, std::string_view type, const db::Rect& r) {
    REQUIRE(db::paint_type(def, *editor.technology, r, this->type(type)).ok());
    db::recompute_bbox(def);
  }

  void triangle(db::CellDef& def, std::string_view type, const db::Rect& r, db::Diagonal diagonal, bool right) {
    REQUIRE(db::paint_type(def, *editor.technology, r, this->type(type), db::TriangleSpec{diagonal, right}).ok());
    db::recompute_bbox(def);
  }

  void label(db::CellDef& def, const Port& p) {
    db::Label l;
    l.text = p.text;
    l.type = p.type.empty() ? tech::kSpace : type(p.type);
    l.rect = p.rect;
    l.sticky = p.sticky;
    if (p.index) {
      l.port_directions = db::kPortNorth | db::kPortEast | db::kPortSouth | db::kPortWest;
      l.port_index = *p.index;
      l.port_class = p.port_class;
      l.port_use = p.use;
      l.port_shape = p.shape;
    }
    def.labels().push_back(l);
    db::recompute_bbox(def);
  }

  /// Places `child` in `parent` at `transform`.
  db::CellUse& place(db::CellDef& parent, db::CellDef& child, const db::Transform& transform = {}) {
    db::PlaceOptions o;
    o.transform = transform;
    auto use = db::place(parent, child, o);
    REQUIRE(use.ok());
    db::recompute_bbox(parent);
    return *use.value();
  }

  void property(db::CellDef& def, std::string_view name, db::PropertyValue value) {
    def.properties().put(name, std::move(value));
  }

  /// lef::write of `def` to `base` (the edit root decides the header).
  cmd::CommandResult write(db::CellDef& def, const std::string& base, LefWriteOptions options = {}) {
    options.files = &services.view().files;
    cmd::CommandResult r = lef::write(editor, def, base, options);
    for (const auto& d : r.diagnostics) {
      messages.push_back(d);
    }
    return r;
  }

  /// write() of `def` as the edit root; returns the file text.
  std::string macro(db::CellDef& def, LefWriteOptions options = {}) {
    editor.edit_root = &def;
    REQUIRE(write(def, def.name(), options).ok());
    return read(def.name() + ".lef");
  }

  /// The text of every message with `code`.
  [[nodiscard]] std::vector<std::string> texts(std::string_view code) const {
    std::vector<std::string> out;
    for (const auto& d : messages) {
      if (d.code == code) {
        out.push_back(d.text);
      }
    }
    return out;
  }

  /// Opens a layout window showing `root`.
  void window(db::CellDef& root) { REQUIRE(editor.windows.open(&root, view::ScreenRect{0, 0, 999, 999}).ok()); }
};

/// The lines of `text` from the line equal to `first` to the line equal to
/// `last`, both included (empty when `first` is missing).
inline std::string block(const std::string& text, std::string_view first, std::string_view last) {
  const std::string head = std::string{first} + "\n";
  std::size_t start = 0;
  if (!text.starts_with(head)) {
    start = text.find('\n' + head);
    if (start == std::string::npos) {
      return {};
    }
    ++start;
  }
  const std::size_t end = text.find(std::string{last} + "\n", start + head.size());
  return end == std::string::npos ? text.substr(start) : text.substr(start, end + last.size() + 1 - start);
}

}  // namespace hefesto::lef::write_test
