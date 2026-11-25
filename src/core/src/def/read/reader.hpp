// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The DEF reader's state and section handlers (private to def/read;
// capability def-read, openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D8).
// Reader::run (top_level.cpp) is DefRead's loop; each section has its own
// file: vias.cpp (DefReadVias), components.cpp (DefReadComponents,
// DefReadLocation), pins.cpp (DefReadPins), nets.cpp (DefReadNets),
// routes.cpp (DefAddRoutes), rules.cpp (DefReadNonDefaultRules,
// DefReadBlockages). geometry.cpp holds the token, number and rectangle
// helpers they share.
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defRead.c, Copyright (C) 1985,
// 1990 Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: DEFR (design D2).

#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/def/read/read.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/lef/lex/statements.hpp"
#include "hefesto/lef/read/layers.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/lef/via/context.hpp"
#include "hefesto/lef/via/generate.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::db {
class CellDef;
class CellLibrary;
}  // namespace hefesto::db

namespace hefesto::drc {
class RuleSet;
}

namespace hefesto::mask {
class OutputStyle;
}

namespace hefesto::tech {
class Technology;
}

namespace hefesto::undo {
class UndoLog;
class ScopedSuspend;
}  // namespace hefesto::undo

namespace hefesto::def::detail {

// Diagnostic codes of the reader (src/core/src/def/read/codes.inc).
inline constexpr std::string_view kError = "DEF-READ-001";
inline constexpr std::string_view kWarning = "DEF-READ-002";
inline constexpr std::string_view kMessage = "DEF-READ-003";
inline constexpr std::string_view kReport = "DEF-READ-004";
inline constexpr std::string_view kCannotOpen = "DEF-READ-005";
inline constexpr std::string_view kLabelMissed = "DEF-READ-006";
inline constexpr std::string_view kFixError = "DEF-READ-007";
inline constexpr std::string_view kFixWarning = "DEF-READ-008";

/// The DEF orientations N, S, E, W, FN, FS, FE, FW (DefReadLocation's
/// table, in its order).
inline constexpr std::string_view kOrientations[] = {"N", "S", "E", "W", "FN", "FS", "FE", "FW"};

/// The base transform of orientation `index` of kOrientations
/// (Geo{Identity,180,90,270,Sideways,UpsideDown,Ref135,Ref45}Transform).
[[nodiscard]] db::Transform orientation_transform(std::size_t index) noexcept;

/// sscanf(token, "%d"): the integer prefix of `token`; nullopt when there
/// is none or it does not fit an int (where sscanf is undefined).
[[nodiscard]] std::optional<std::int32_t> parse_int(std::string_view token) noexcept;

/// The whole token is a decimal integer (magic-compat OFF strict parses).
[[nodiscard]] std::optional<std::int32_t> parse_whole_int(std::string_view token) noexcept;

/// `r` clipped to the plane's extent (a coordinate beyond it cannot be
/// painted); nullopt when nothing of it is left.
[[nodiscard]] std::optional<db::Rect> clip_to_plane(const db::Rect& r) noexcept;

/// What the reader works on, taken from the session at the start of a read.
struct ReadContext {
  db::CellLibrary& cells;
  const tech::Technology& tech;
  compat::CompatSettings compat;
  const mask::OutputStyle* style = nullptr;  // CIFCurStyle
  const drc::RuleSet* rules = nullptr;       // the current DRC style
  lef::LayerTable& layers;
  lef::ViaTable& vias;
  lef::RuleTable& rules_table;
  std::int64_t lambda_internal = 1;  // DBLambda[1]
  std::int64_t lambda_lambda = 1;    // DBLambda[0]
  std::size_t font_count = 0;        // rendering fonts loaded
  db::CellDef* edit_cell = nullptr;  // EditCellUse's definition (annotation, Q-LEFDEF-81 ON)
  const DefReadOptions& options;
  undo::UndoLog* undo = nullptr;  // suspended once the read creates its design cell
  std::string file_argument;  // the name `def read` was given (DefNewCell(inName))
};

/// LefNextToken over the lexer, with tokens pushed back in front of it
/// (the readers' one-token look-ahead and the replay of a whole net
/// statement, Q-LEFDEF-74 OFF).
class TokenSource {
 public:
  explicit TokenSource(lef::Lexer& lexer) : lexer_(lexer) {}

  /// The next token; nullopt at the end of the input.
  std::optional<std::string> next();
  /// The next token, or "" at the end of the input (where Magic reads a
  /// NULL token; every loop also tests ended()).
  std::string take();
  /// `token` is read again next.
  void push_front(std::string token) { pending_.push_front(std::move(token)); }
  /// `tokens` are read again next, in their order.
  void push_front(std::vector<std::string> tokens);
  [[nodiscard]] bool ended() const noexcept { return ended_ && pending_.empty(); }
  [[nodiscard]] lef::Lexer& lexer() noexcept { return lexer_; }

 private:
  lef::Lexer& lexer_;
  std::deque<std::string> pending_;
  bool ended_ = false;
};

/// The LEF/DEF entry of a name, as Magic's LefInfo holds it: a via
/// definition (DEF or LEF), which shadows a tech `lef` layer of the same
/// name, or a layer record. Both null: no entry.
struct LefEntry {
  const lef::LayerRecord* layer = nullptr;
  const lef::ViaDefinition* via = nullptr;

  [[nodiscard]] bool empty() const noexcept { return layer == nullptr && via == nullptr; }
  friend bool operator==(const LefEntry&, const LefEntry&) = default;
};

/// One rectangle a route statement emits (internal units), or a route
/// polygon (Q-LEFDEF-70 OFF).
struct RouteShape {
  tech::TileType type = tech::kSpace;
  db::Rect rect;
  std::vector<db::Point> polygon;  // non-empty: paint this polygon instead
};

class Reader {
 public:
  Reader(lef::Lexer& lexer, lef::MessageCounter& counter, diag::DiagnosticSink& out, const ReadContext& context);
  ~Reader();
  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;

  /// DefRead's loop; returns the design cell (nullptr when none was chosen).
  db::CellDef* run();

  // --- tokens and messages (geometry.cpp) ---
  std::optional<std::string> next() { return tokens_.next(); }
  std::string take() { return tokens_.take(); }
  void push_front(std::string token) { tokens_.push_front(std::move(token)); }
  void push_front(std::vector<std::string> tokens) { tokens_.push_front(std::move(tokens)); }
  [[nodiscard]] bool ended() const noexcept { return tokens_.ended(); }
  /// LefEndStatement: tokens through the next `;`.
  void skip_statement();
  /// LefSkipSection(f, name).
  void skip_section(std::string_view name);
  /// LefParseEndStatement(f, name) != 0.
  bool end_matches(std::string_view name);

  void error(std::string_view text) const;    // LefError(DEF_ERROR)
  void warning(std::string_view text) const;  // LefError(DEF_WARNING)
  void message(std::string_view text) const;  // LefError(DEF_INFO)
  void lef_error(std::string_view text) const;  // LefError(LEF_ERROR), shared LEF helpers
  void fix_error(std::string_view text) const;    // Hefesto-only (a fixed crash or undefined value)
  void fix_warning(std::string_view text) const;  // Hefesto-only
  void report(std::string text) const;          // TxPrintf
  /// DBCellRead of a master through the options' read_master; its messages
  /// join the read's at this point. False when there is no reader or the
  /// read fails.
  bool read_master(db::CellDef& def) const;
  void progress() const;                        // LefEstimate: the progress host
  /// LefEstimate of one item of a counted section (`total` declared): the
  /// progress host, and with `total` > 0 Magic's display status leaves
  /// DISPLAY_SUSPEND (EditorState::display_status_active).
  void estimate(std::int32_t total);
  [[nodiscard]] bool estimated() const noexcept { return estimated_; }
  [[nodiscard]] bool cancelled() const noexcept;

  // --- units and geometry (geometry.cpp) ---
  [[nodiscard]] float oscale() const noexcept { return oscale_; }
  /// DEF units per micron (oscale / s0: the UNITS value, or 1 before one);
  /// 0 when s0 is not positive.
  [[nodiscard]] double def_units_per_micron() const noexcept {
    return s0_ > 0.0F ? static_cast<double>(oscale_) / static_cast<double>(s0_) : 0.0;
  }
  /// round(v / oscale); nullopt when it does not fit (Magic's int cast is
  /// undefined there).
  [[nodiscard]] std::optional<db::Coord> scaled(float value) const noexcept;
  /// round(2v / oscale): doubled units.
  [[nodiscard]] std::optional<db::Coord> doubled(float value) const noexcept;
  /// LefReadRect(f, layer, scale) (scale is oscale, or oscale/2 for via
  /// geometry). `semicolon` is set when the parse consumed a `;` token.
  std::optional<db::Rect> read_rect(bool layer_known, float scale, bool& semicolon);
  /// The `( x y )` points that follow, converted with round(v / scale);
  /// the first token that does not open a point is read again next.
  /// nullopt when a point does not parse (it is consumed).
  std::optional<std::vector<db::Point>> read_points(float scale);
  /// LefReadLayer(f, obstruct): the layer of the next token.
  std::optional<tech::TileType> read_layer(bool obstruction);
  /// The LefInfo entry of `name`.
  [[nodiscard]] LefEntry lookup(std::string_view name) const;
  /// HashLookOnly found the name with no record (a released via name).
  [[nodiscard]] bool named_without_record(std::string_view name) const;
  /// A key of Magic's LefInfo (the layer table's key order or the via
  /// table) with no layer or via record.
  [[nodiscard]] bool keyed_without_record(std::string_view name) const;
  /// LefHelper_DBTechNameType_LefLower: the magic type named by `name` in
  /// lower case.
  [[nodiscard]] std::optional<tech::TileType> lower_type(std::string_view name) const;
  /// The default route width of `entry` (lefl->info.route.width, or
  /// DEFAULT_WIDTH * DBLambda[1] / DBLambda[0] without one), with a
  /// non-positive width replaced by the DRC default width of `type` in
  /// both modes (Q-LEFDEF-1, tier 1).
  [[nodiscard]] db::Coord default_width(const LefEntry& entry, tech::TileType type) const;
  /// DRCGetDefaultLayerWidth.
  [[nodiscard]] std::int32_t drc_width(tech::TileType type) const;
  /// The via code's context; its messages take their prefix from
  /// lef::via_message_dialect.
  [[nodiscard]] lef::ViaContext via_context() noexcept;

  // --- the design cell (top_level.cpp) ---
  /// The design cell, created under the fallback name first (Q-LEFDEF-78).
  db::CellDef& design();
  void paint(db::CellDef& cell, tech::TileType type, const db::Rect& rect) const;
  /// "  Processed N <noun> total." when `processed` equals `total`, else
  /// the count-mismatch warning naming `noun`.
  void count_check(std::int32_t processed, std::int32_t total, std::string ok_text, std::string_view noun);
  /// The port index of the next pin port label (pinNum).
  [[nodiscard]] std::uint32_t& pin_number() noexcept { return pin_number_; }

  [[nodiscard]] const ReadContext& context() const noexcept { return context_; }
  [[nodiscard]] bool magic_compat() const noexcept { return context_.compat.magic_compat; }
  [[nodiscard]] const lef::MessageTarget& lef_messages() const noexcept { return lef_messages_; }

  /// The cell this read created (nullptr when the design cell existed).
  [[nodiscard]] db::CellDef* created() const noexcept { return created_; }
  /// The FIXED_BBOX values the read replaced on an existing design cell.
  struct PropertyChange {
    bool changed = false;
    std::optional<db::Rect> before;
  };
  [[nodiscard]] const PropertyChange& die_change() const noexcept { return die_change_; }

  // --- sections ---
  void vias(std::int32_t total);                             // vias.cpp
  void components(std::int32_t total);                       // components.cpp
  void pins(std::int32_t total);                             // pins.cpp
  void nets(std::int32_t total, bool special);               // nets.cpp
  void nondefault_rules(std::int32_t total);                 // rules.cpp
  void blockages(std::int32_t total);                        // rules.cpp
  /// DefReadLocation: the transform of `( X Y ) orient`, or with
  /// `unplaced` the origin; `bbox` is the master's box (null for pins).
  std::optional<db::Transform> read_location(const db::Rect& bbox, bool unplaced, std::string_view use_name);

  /// DefAddRoutes (routes.cpp): reads one routing statement and paints it;
  /// returns the token that ended it (`;` or `+`, or "" at the end).
  struct RouteRequest {
    bool special = false;
    std::optional<std::string> label;  // the net name when the route is labelled
    const lef::NonDefaultRule* rules = nullptr;
  };
  std::string add_routes(const RouteRequest& request);

  /// Annotation (Q-LEFDEF-81): a net-name label on the paint under
  /// terminal `component/pin`; true when one was placed.
  bool annotate_terminal(const std::string& component, const std::string& pin, const std::string& net);
  void paint_routes(const std::vector<RouteShape>& shapes, const std::optional<std::string>& label);
  [[nodiscard]] db::Rect polygon_area(db::CellDef& cell, const RouteShape& shape, bool annotate) const;
  [[nodiscard]] bool on_paint_of(const db::CellDef& cell, tech::TileType type, const db::Rect& area) const;

 private:
  bool top_statement(const std::string& keyword);
  bool counted_section(std::size_t section);
  void units();
  void design_statement();
  void technology_statement();
  void die_area();
  std::optional<db::Rect> die_rect_all_points();
  void set_die_area(const db::Rect& die);
  [[nodiscard]] std::string fallback_name() const;
  db::CellDef& new_cell(const std::string& name);
  void via_definition(bool& generated);
  void blockage();
  bool blockage_rect(std::optional<tech::TileType> layer, bool has_layer);

  lef::Lexer& lexer_;
  lef::MessageCounter& counter_;
  diag::DiagnosticSink& out_;
  const ReadContext& context_;
  TokenSource tokens_;
  lef::MessageTarget lef_messages_;
  class ViaSink;
  std::unique_ptr<diag::DiagnosticSink> via_sink_;

  float s0_ = 1.0F;      // CIFGetOutputScale(1000)
  float oscale_ = 1.0F;  // DEF units per internal unit
  bool units_seen_ = false;
  db::CellDef* design_ = nullptr;
  db::CellDef* created_ = nullptr;
  std::unique_ptr<undo::ScopedSuspend> suspend_;
  PropertyChange die_change_;

  // Generated-via parameters kept across the definitions of the read
  // (magic-compat ON keeps the last value, defRead.c:1916-1921).
  lef::ViaParamSet via_params_;

  bool estimated_ = false;  // a LefEstimate call with a declared count

  // PINS state that lives across pins (magic-compat ON, Q-LEFDEF-64, 65).
  std::uint32_t pin_number_ = 0;
};

}  // namespace hefesto::def::detail
