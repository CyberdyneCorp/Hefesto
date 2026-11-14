// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A recording CifReadTarget for the CIF parser tests: every call becomes
// one line of text, so a test compares the event sequence the parser drove.
// Owner: CIF.

#pragma once

#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "hefesto/cif/read/messages.hpp"
#include "hefesto/cif/read/parser.hpp"
#include "hefesto/diag/sink.hpp"

namespace hefesto::cif::test {

inline std::string rect_text(const db::Rect& r) {
  return "(" + std::to_string(r.x0) + "," + std::to_string(r.y0) + ")-(" + std::to_string(r.x1) + "," +
         std::to_string(r.y1) + ")";
}

inline std::string shape_text(const mask::ReadShape& shape) {
  if (const auto* r = std::get_if<db::Rect>(&shape)) {
    return "rect " + rect_text(*r);
  }
  std::string out = "poly";
  for (const db::Point& p : std::get<mask::Polygon>(shape)) {
    out += " (" + std::to_string(p.x) + "," + std::to_string(p.y) + ")";
  }
  return out;
}

inline std::string transform_text(const db::Transform& t) {
  return std::to_string(t.a) + " " + std::to_string(t.b) + " " + std::to_string(t.c) + " " + std::to_string(t.d) +
         " " + std::to_string(t.e) + " " + std::to_string(t.f);
}

class RecordingTarget final : public CifReadTarget {
 public:
  std::vector<std::string> events;
  std::set<std::string, std::less<>> library{"top"};  // cells that exist before the read
  std::string edit_cell = "top";

  diag::Status begin_cell(std::string_view name) override {
    library.insert(std::string{name});
    events.push_back("begin " + std::string{name});
    return diag::Done{};
  }
  diag::Status paint(std::size_t cif_layer, const mask::ReadShape& shape) override {
    events.push_back("paint " + std::to_string(cif_layer) + " " + shape_text(shape));
    return diag::Done{};
  }
  diag::Status label(const mask::ReadLabel& /*label*/) override {
    events.emplace_back("unexpected ReadSink::label");
    return diag::Done{};
  }
  diag::Status call(std::string_view /*child*/, const db::Transform& /*transform*/,
                    const db::ArrayInfo& /*array*/) override {
    events.emplace_back("unexpected ReadSink::call");
    return diag::Done{};
  }
  diag::Status property(std::string_view /*key*/, std::string_view /*value*/) override {
    events.emplace_back("unexpected ReadSink::property");
    return diag::Done{};
  }
  diag::Status end_cell() override {
    events.emplace_back("end");
    return diag::Done{};
  }
  bool cell_exists(std::string_view name) const override { return library.contains(name); }
  std::string edit_cell_name() const override { return edit_cell; }
  diag::Status rename_cell(std::string_view from, std::string_view to) override {
    if (to.empty() || library.contains(to)) {
      return diag::make("CIF-READ-001", "rename refused");
    }
    library.erase(std::string{from});
    library.insert(std::string{to});
    if (from == edit_cell) {
      edit_cell = std::string{to};
    }
    events.push_back("rename " + std::string{from} + " " + std::string{to});
    return diag::Done{};
  }
  diag::Status rescale(std::int64_t factor) override {
    events.push_back("rescale " + std::to_string(factor));
    return diag::Done{};
  }
  diag::Status place(const CifCall& c) override {
    library.insert(c.child);  // a forward call creates the cell
    std::string text = "place " + c.child + " [" + transform_text(c.transform) + "]";
    if (!c.translation_in_read_units) {
      text += " unscaled";
    }
    if (!c.use_id.empty()) {
      text += " id=" + c.use_id;
    }
    text += c.child_defined ? " defined" : " forward";
    events.push_back(text);
    return diag::Done{};
  }
  diag::Status cif_label(const CifLabel& l) override {
    std::string text = "label " + l.label.text + " " + rect_text(l.label.area) + " type=" + std::to_string(l.type);
    if (l.sticky) {
      text += " sticky";
    }
    events.push_back(text);
    return diag::Done{};
  }
  diag::Status end_top() override {
    events.emplace_back("end_top");
    return diag::Done{};
  }
};

/// The default style of the tests: CMF (label type 3), CPG (4), CX (labels
/// discarded), CMFP (type 3, port kind so sticky), and CNW, a name of
/// another style.
inline ParseStyle test_style() {
  ParseStyle s;
  s.name = "lambda";
  s.multiplier = 1;
  s.layers = {
      ParseLayer{"CMF", true, 3, false},
      ParseLayer{"CPG", true, 4, false},
      ParseLayer{"CX", true, kDiscardLabels, false},
      ParseLayer{"CMFP", true, 3, true},
      ParseLayer{"CNW", false, 0, false},
  };
  s.rule_count = 4;
  return s;
}

struct Reader {
  ParseStyle style = test_style();
  ParseOptions options;
  RecordingTarget target;
  diag::CollectingSink console;
  diag::CollectingSink redirect_file;
  ReadSession session;
  mask::WarningLevel level = mask::WarningLevel::warn_default;
  bool use_redirect_file = true;

  /// Parses `text` and returns the target's events.
  std::vector<std::string> read(std::string_view text) {
    ReadMessages messages(level, options.magic_compat, console, use_redirect_file ? &redirect_file : nullptr);
    parse(text, style, options, target, messages, session);
    return target.events;
  }

  /// The texts of the console diagnostics.
  [[nodiscard]] std::vector<std::string> printed() const {
    std::vector<std::string> out;
    for (const auto& d : console.snapshot()) {
      out.push_back(d.text);
    }
    return out;
  }

  [[nodiscard]] bool printed_line(std::string_view text) const {
    const auto lines = printed();
    return std::find(lines.begin(), lines.end(), text) != lines.end();
  }

  [[nodiscard]] std::vector<std::string> codes() const {
    std::vector<std::string> out;
    for (const auto& d : console.snapshot()) {
      out.push_back(d.code);
    }
    return out;
  }
};

/// The events from the first "paint"/"label"/"place"/... line on, without
/// the trailing "end_top".
inline std::vector<std::string> body(std::vector<std::string> events) {
  if (!events.empty() && events.back() == "end_top") {
    events.pop_back();
  }
  return events;
}

}  // namespace hefesto::cif::test
