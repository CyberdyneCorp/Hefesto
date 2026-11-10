// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `wiring` technology section, the DRC default width of a layer (read
// from the compiled DRC rule set, drc/rules/query.hpp), and the wiring undo event
// (wiring-tool "wiring technology section", "Wiring tool buttons").
// Ported from Magic VLSI 8.3.694 (fc5daa9e) wiring/wireTech.c,
// wiring/wireUndo.c and drc/DRCtech.c:4620-4664 (DRCGetDefaultLayerWidth),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md). Owner: WIRE.

#include "hefesto/wiring/wiring.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/drc/rules/cif_catalog.hpp"
#include "hefesto/drc/rules/query.hpp"
#include "hefesto/drc/rules/style.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/undo/undo_log.hpp"
#include "hefesto/wiring/diag_codes.hpp"
#include "wiring/text.hpp"

namespace hefesto::wiring {

namespace {

// --- wiring section ------------------------------------------------------------

struct LineReader {
  const tech::RawLine& line;
  const tech::Technology& tech;
  const tech::GridScale& grid;
  diag::DiagnosticSink* sink;

  void error(std::string text) const {
    if (sink != nullptr) {
      sink->emit(diag::make(codes::kTechLine, std::move(text), diag::SourceLocation{line.file, line.line}));
    }
  }

  // DBTechNoisyNameType.
  [[nodiscard]] std::optional<tech::TileType> type(const std::string& name) const {
    const auto found = tech.find_type(name.substr(0, name.find('/')));
    if (found.ok()) {
      return found.id;
    }
    error(std::string{found.error == tech::LookupError::ambiguous ? "Ambiguous" : "Unrecognized"} +
          " layer (type) name \"" + name + "\"");
    return std::nullopt;
  }

  // An integer field scaled to the internal grid (WireTechScale: multiply,
  // then truncating divide); nullopt (with `what` reported) when not an int.
  [[nodiscard]] std::optional<db::Coord> dimension(const std::string& text, std::string_view what) const {
    if (!str_is_int(text)) {
      error(std::string{what} + " must be an integer.");
      return std::nullopt;
    }
    const std::int64_t v = std::int64_t{c_atoi(text)} * grid.internal / std::max<std::int64_t>(grid.lambda, 1);
    return static_cast<db::Coord>(v);
  }
};

void read_scalefactor(const LineReader& r, WiringSection& section) {
  const auto& t = r.line.tokens;
  if (t.size() != 2) {
    r.error("\"scalefactor\" line must have exactly 2 arguments.");
  } else if (!str_is_int(t[1])) {
    r.error("\"scalefactor\" argument must be an integer.");
  } else {
    section.scale_factor = c_atoi(t[1]);
  }
}

std::optional<WiringContact> read_contact(const LineReader& r) {
  const auto& t = r.line.tokens;
  if (t.size() != 7 && t.size() != 9) {
    r.error("\"contact\" lines must have exactly 7 or 9 arguments.");
    return std::nullopt;
  }
  const std::size_t ext = t.size() == 9 ? 1 : 0;
  // Magic resolves all three names before checking any of them.
  const auto type = r.type(t[1]);
  const auto layer1 = r.type(t[3]);
  const auto layer2 = r.type(t[5 + ext]);
  if (!type || !layer1 || !layer2) {
    return std::nullopt;
  }
  WiringContact c{*type, 0, *layer1, 0, 0, *layer2, 0, 0};
  const auto size = r.dimension(t[2], "Contact size");
  const auto s1 = size ? r.dimension(t[4], "Contact surround distance") : std::nullopt;
  const auto s2 = s1 ? r.dimension(t[6 + ext], "Contact surround distance") : std::nullopt;
  if (!s2) {
    return std::nullopt;
  }
  c.size = *size;
  c.surround1 = *s1;
  c.surround2 = *s2;
  if (ext != 0) {
    const auto e1 = r.dimension(t[5], "Contact extend distance");
    const auto e2 = e1 ? r.dimension(t[8], "Contact extend distance") : std::nullopt;
    if (!e2) {
      return std::nullopt;
    }
    c.extend1 = *e1;
    c.extend2 = *e2;
  }
  return c;
}

// x / n rounded up for the positive factors Magic uses (con_x / WireUnits,
// plus one when there is a remainder; C truncation otherwise).
db::Coord divide_up(db::Coord value, std::int64_t n) {
  if (n == 0) {
    return value;  // Magic would divide by zero; keep the value
  }
  const std::int64_t q = value / n;
  return static_cast<db::Coord>(value % n != 0 ? q + 1 : q);
}

// --- undo ------------------------------------------------------------------------

class WireEvent final : public undo::Event {
 public:
  WireEvent(const WireParams& before, const WireParams& after) : before_(before), after_(after) {}

  void undo(editor::EditorState& editor) override { restore(editor, before_); }
  void redo(editor::EditorState& editor) override { restore(editor, after_); }
  [[nodiscard]] std::string describe() const override { return "wiring parameters"; }

 private:
  static void restore(editor::EditorState& editor, const WireParams& p) {
    editor.wire.set_params(p);
    editor.wire.remembered = p;
  }

  WireParams before_;
  WireParams after_;
};

}  // namespace

WiringContact WiringSection::scaled(const WiringContact& contact) const {
  WiringContact c = contact;
  c.size = divide_up(c.size, scale_factor);
  c.surround1 = divide_up(c.surround1, scale_factor);
  c.surround2 = divide_up(c.surround2, scale_factor);
  c.extend1 = divide_up(c.extend1, scale_factor);
  c.extend2 = divide_up(c.extend2, scale_factor);
  return c;
}

WiringSection parse_wiring_lines(std::span<const tech::RawLine> lines, const tech::Technology& tech,
                                 const tech::GridScale& grid, diag::DiagnosticSink* sink) {
  WiringSection section;
  for (const tech::RawLine& line : lines) {
    if (line.tokens.empty()) {
      continue;
    }
    const LineReader reader{line, tech, grid, sink};
    if (line.tokens[0] == "scalefactor") {
      read_scalefactor(reader, section);
    } else if (line.tokens[0] != "contact") {
      reader.error("Unknown wiring keyword: " + line.tokens[0] + ".  Line ignored.");
    } else if (auto contact = read_contact(reader)) {
      section.contacts.insert(section.contacts.begin(), *contact);
    }
  }
  return section;
}

diag::Outcome<WiringSection> parse_wiring_section(const tech::Technology& tech, const tech::GridScale& grid,
                                                  diag::DiagnosticSink* sink) {
  WiringSection section;
  for (const tech::RawSection& raw : tech.raw_sections()) {
    if (raw.name == "wiring") {
      section = parse_wiring_lines(raw.lines, tech, grid, sink);
    }
  }
  return section;
}

db::Coord default_layer_width(const tech::Technology& tech, tech::TileType type, const tech::GridScale& grid) {
  // DRCGetDefaultLayerWidth over the compiled default style (m3-drc design
  // D5): the session's rule set gives the same value without recompiling
  // (drc::default_width(*editor.drc.rules, tech, type)).
  diag::NullSink quiet;
  const drc::CifCatalog cif = drc::CifCatalog::from_technology(tech, quiet);
  const drc::DrcStyleList styles = drc::style_list(tech);
  const auto rules = drc::load_style(tech, styles.default_style(), grid, cif, compat::CompatSettings{}, quiet);
  return drc::default_width(*rules, tech, type);
}

std::unique_ptr<undo::Event> make_wire_event(const WireParams& before, const WireParams& after) {
  return std::make_unique<WireEvent>(before, after);
}

void remember_for_undo(editor::EditorState& editor) {
  const WireParams now = editor.wire.params();
  editor.undo.record(make_wire_event(editor.wire.remembered, now));
  editor.wire.remembered = now;
}

}  // namespace hefesto::wiring
