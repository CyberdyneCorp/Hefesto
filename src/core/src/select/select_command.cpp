// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `select` command (cmd-selection "Select qualifiers", "Select options",
// "Area and visible selection", "Point selection and cycling",
// "Box-anchored chunk, region and net selection", "Cell selection",
// "Intersection selection", "Selection utility options", "Floating
// selection"). Owner: SELECT.
// (Magic ref: commands/CmdRS.c:540-1735 CmdSelect, cmdSelectArea,
// cmdIntersectArea)

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cell/commands.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/cell/hier.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/editor/edit_frame.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/select/diag_codes.hpp"
#include "select/commands_internal.hpp"
#include "select/ops.hpp"

namespace hefesto::select {

namespace cu = db::cmdutil;
using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;
using cmd::Value;
using ops::Context;

namespace {

enum Option : int {
  kArea, kVisible, kCell, kLabels, kIntersection, kClear, kFlat, kHelp, kKeep, kMove, kPick, kSave, kFeedback,
  kBbox, kBox, kChunk, kRegion, kNet, kShort, kDefault
};

constexpr std::array<std::string_view, 19> kOptions = {
    "area", "visible", "cell", "labels", "intersection", "clear", "flat", "help", "keep", "move",
    "pick", "save", "feedback", "bbox", "box", "chunk", "region", "net", "short"};

// One help line per option, the point selection first (cmdSelectMsg).
constexpr std::array<std::string_view, 20> kHelpLines = {
    "[more | less | nocycle] [layers] [at x y]\n"
    "                                [de]select paint chunk/region/net under\n"
    "                                cursor, or [de]select subcell if cursor\n"
    "\t                         over space",
    "[more | less] area [layers]     [de]select all info under box in layers",
    "[more | less] visible [layers]  [de]select all visible info under box in layers",
    "[more | less | top] cell [name] [de]select cell under cursor, or \"name\"",
    "[do | no] labels\t\t [do not] select subcell labels",
    "[more | less] intersection [layers]\t[de]select intersection of layers",
    "clear                           clear selection",
    "flat\t\t\t\t flatten the contents of the selection",
    "help                            print this message",
    "keep                            copy the selection to the layout",
    "move                            move selection to a location in the layout",
    "pick                            delete selection from layout",
    "save file                       save selection on disk in file.mag",
    "feedback [style]\t\t copy selection to feedback",
    "bbox\t\t\t\t return the bounding box of the selection",
    "[more | less] box | chunk | region | net [layers]\n"
    "\t\t\t\t [de]select chunk/region/net specified by\n"
    "\t\t\t\t the lower left corner of the current box",
    "short name1 name2\t\t find shorting path between two labels",
};

/// The parsed command line (Magic's isqual / optionArgs / primargs).
struct Parsed {
  std::vector<std::string> argv;  // command name first
  bool more = false;
  bool less = false;
  bool same_place = true;
  bool force_same = false;  // `same`
  bool dotop = false;
  int isqual = 0;
  connect::LabelPolicy policy = connect::LabelPolicy::full;
  bool policy_given = false;  // `do`, `no` or `simple`
  std::optional<db::Point> at;  // root coordinates
  int option = kDefault;
  std::size_t opt = 1;      // index in argv of optionArgs[0]
  std::size_t primargs = 1;
  [[nodiscard]] const std::string* arg(std::size_t i) const {
    return opt + i < argv.size() ? &argv[opt + i] : nullptr;
  }
};

enum Qualifier : int { kMore, kLess, kNocycle, kSame, kDo, kNo, kSimple, kTop, kNone };

[[nodiscard]] bool is_prefix(std::string_view word, std::string_view of) {
  return !word.empty() && word.size() <= of.size() && of.substr(0, word.size()) == word;
}

/// "Select qualifiers": prefix matching in Magic's order; with magic-compat
/// OFF an exact match wins first (Q-CMDEDIT-60).
Qualifier match_qualifier(const Parsed& p, bool magic_compat) {
  static constexpr std::array<std::string_view, 8> kWords = {"more", "less", "nocycle", "same",
                                                             "do",   "no",   "simple",  "top"};
  const std::string& w = p.argv[1];
  const auto top_ok = [&] { return p.argv.size() >= 3 && is_prefix(p.argv[2], "cell"); };
  if (!magic_compat) {
    for (std::size_t i = 0; i < kWords.size(); ++i) {
      if (w == kWords[i] && (i != kTop || top_ok())) {
        return static_cast<Qualifier>(i);
      }
    }
  }
  for (std::size_t i = 0; i < kWords.size(); ++i) {
    if (is_prefix(w, kWords[i])) {
      return i == kTop && !top_ok() ? kNone : static_cast<Qualifier>(i);
    }
  }
  return kNone;
}

void apply_qualifier(Parsed& p, Qualifier q, Selection& sel) {
  p.isqual = q == kNone ? 0 : 1;
  switch (q) {
    case kMore:
      p.more = true;
      break;
    case kLess:
      p.less = true;
      break;
    case kNocycle:
      p.same_place = false;
      p.policy = connect::LabelPolicy::none;
      sel.cycle.type = -1;  // avoid cycling between types
      break;
    case kSame:
      p.force_same = true;
      break;
    case kNo:
      p.policy = connect::LabelPolicy::none;
      p.policy_given = true;
      break;
    case kSimple:
      p.policy = connect::LabelPolicy::simple;
      p.policy_given = true;
      break;
    case kTop:
      p.dotop = true;
      break;
    case kDo:
      p.policy_given = true;
      break;
    case kNone:
      break;
  }
  p.opt = p.isqual != 0 ? 2 : 1;
}

/// The trailing `at X Y` (edit coordinates, moved to the root).
diag::Status take_at(Parsed& p, const Context& c) {
  const std::size_t argc = p.argv.size();
  if (argc - static_cast<std::size_t>(p.isqual) <= 3 || p.argv[argc - 3] != "at") {
    return diag::Done{};
  }
  auto x = cu::parse_coord(c.s, p.argv[argc - 2]);
  auto y = cu::parse_coord(c.s, p.argv[argc - 1]);
  if (!x.ok() || !y.ok()) {
    return !x.ok() ? x.error() : y.error();
  }
  p.at = editor::to_root(*c.ed, db::Point{x.value(), y.value()});
  p.argv.resize(argc - 3);
  return diag::Done{};
}

CommandResult help(const Context& c) {
  std::string out = "Selection commands are:";
  for (const std::string_view line : kHelpLines) {
    if (line.empty()) {
      break;  // the NULL that ends cmdSelectMsg (the padding is for usage_error)
    }
    out += "\n    select " + std::string{line};
  }
  cu::print(c.s, std::move(out));
  return CommandResult::success();
}

CommandResult usage_error(int option) {
  return fail_text(codes::kUsage,
                   "Bad arguments:\n    select " + std::string{kHelpLines[static_cast<std::size_t>(option) + 1]});
}

/// Picks the option (Magic: Lookup, an unknown single word is a layer list).
std::optional<CommandResult> choose_option(Parsed& p, const Context& c) {
  p.primargs = p.argv.size() - static_cast<std::size_t>(p.isqual);
  if (p.primargs == 1) {
    p.option = kDefault;
    return std::nullopt;
  }
  p.option = cu::lookup_in(p.argv[p.opt], kOptions);
  if (p.option < 0 && p.primargs != 2) {
    // CmdSelect: TxError, the help and a normal return (no Tcl error), so a
    // script goes on (CMDEDIT-SEL-07).
    cu::warn(c.s, "\"" + p.argv[1] + "\" isn't a valid select option.");
    return help(c);
  }
  if (p.option < 0) {
    p.option = kDefault;
    p.opt = (p.more || p.less || !p.same_place) ? 1 : 0;
  }
  if (p.option != kBox && p.option != kChunk && p.option != kRegion && p.option != kNet) {
    c.ed->selection.cycle.level = CycleState::Level::none;
  }
  return std::nullopt;
}

// ---- area, visible, intersection ----------------------------------------------

CommandResult area_select(const Context& c, const Parsed& p) {
  const std::size_t limit = p.option == kArea ? 4 : 3;
  if (p.primargs > limit) {
    return usage_error(p.option);
  }
  const std::string glob = p.option == kArea && p.primargs == 4 ? *p.arg(2) : std::string{};
  Snapshot before = ops::begin(c);
  if (!(p.more || p.less)) {
    c.ed->selection.clear();
  }
  const auto box = ops::root_box(c);
  if (!box) {
    ops::commit(c, std::move(before));
    cu::print(c.s, "The box isn't in a window.");
    return CommandResult::success();
  }
  auto layers = ops::parse_layers(c, p.primargs >= 3 ? *p.arg(1) : std::string{"*,labels,subcell"});
  if (!layers.ok()) {
    ops::commit(c, std::move(before));
    return cu::fail(layers.error());
  }
  ops::Layers l = layers.value();
  const std::string* g = glob.empty() ? nullptr : &glob;
  if (p.less) {
    ops::remove_area(c, *box, l, g);
  } else {
    if (p.option == kVisible) {
      l.types &= c.visible;
      l.labels = l.labels && c.see_labels;
      l.cells = l.cells && c.see_cells;
    } else {
      l.labels = true;
    }
    ops::select_area(c, *box, l, g);
  }
  ops::commit(c, std::move(before));
  return CommandResult::success();
}

CommandResult intersection(const Context& c, const Parsed& p) {
  if (p.primargs != 3) {
    return usage_error(kIntersection);
  }
  const auto box = ops::root_box(c);
  if (!box) {
    cu::print(c.s, "The box isn't in a window.");
    return CommandResult::success();
  }
  std::string_view name = *p.arg(1);
  const bool negate = !name.empty() && (name.front() == '~' || name.front() == '!');
  if (negate) {
    name.remove_prefix(1);
  }
  const auto type = c.tech->find_type(name);
  if (!type.ok()) {
    return fail_text(codes::kBadLayer, "Cannot parse layer type \"" + *p.arg(1) + "\".");
  }
  Snapshot before = ops::begin(c);
  ops::select_intersection(c, *box, *type.id, negate);
  ops::commit(c, std::move(before));
  return CommandResult::success();
}

// ---- utilities ------------------------------------------------------------------

CommandResult bbox(const Context& c) {
  const db::Rect r = c.ed->selection.bbox();
  const db::Rect b = r.is_null() ? db::Rect{} : r;
  return CommandResult::success(Value{Value::List{cu::coord_value(c.s, b.x0), cu::coord_value(c.s, b.y0),
                                                  cu::coord_value(c.s, b.x1), cu::coord_value(c.s, b.y1)}});
}

// The floating offset of `select move`: from the box's lower-left corner to
// the pointer (two arguments) or the explicit X Y.
diag::Outcome<db::Point> floating_offset(const Context& c, const Parsed& p) {
  if (p.primargs == 2) {
    const auto box = ops::root_box(c);
    const auto& point = c.ed->command_point;
    const db::CellDef* edit_root = c.ed->edit_root != nullptr ? c.ed->edit_root : c.ed->edit_cell;
    if (!point || !box || edit_root != c.root) {
      return diag::make(codes::kNotInEditCell, "Error:  Pointer is not in the edit cell.");
    }
    return db::Point{point->point.x - box->x0, point->point.y - box->y0};
  }
  auto x = cu::parse_coord(c.s, p.argv[2]);
  auto y = cu::parse_coord(c.s, p.argv[3]);
  if (!x.ok() || !y.ok()) {
    return !x.ok() ? x.error() : y.error();
  }
  return db::Point{x.value(), y.value()};
}

CommandResult move_floating(const Context& c, const Parsed& p) {
  if (p.more || p.less || (p.primargs != 4 && p.primargs != 2)) {
    return usage_error(kMove);
  }
  const auto offset = floating_offset(c, p);
  if (!offset.ok()) {
    return p.primargs == 2 ? CommandResult::failure(offset.error()) : cu::fail(offset.error());
  }
  Snapshot before = c.ed->selection.snapshot();
  c.ed->selection.set_floating_offset(offset.value());
  ops::commit(c, std::move(before));
  return CommandResult::success();
}

CommandResult save(const Context& c, CommandContext& ctx, const Parsed& p) {
  if (p.primargs != 3) {
    return usage_error(kSave);
  }
  db::CellDef* def = c.ed->selection.def();
  if (def == nullptr) {
    return fail_text(codes::kEmpty, "Nothing is selected.");
  }
  (void)db::adjust_labels(*def, *c.tech, db::whole_plane());
  db::recompute_bbox(*def);
  drc::paint_check_area(*c.tech, *def, def->bbox());  // SEL_SAVE: DRC check information is saved with it
  std::string file = *p.arg(1);
  if (file.find('.') == std::string::npos) {
    file += ".mag";
  }
  return cell::save_def_as(ctx, *def, file);
}

CommandResult feedback(const Context& c, const Parsed& p) {
  view::FeedbackStyle style{46};  // STYLE_ORANGE1
  if (p.primargs > 2) {
    std::optional<std::size_t> found;
    if (c.ed->styles) {
      found = display::style_from_name(c.ed->styles->table, *p.arg(1));
    }
    if (cu::str_is_int(*p.arg(1))) {
      found = static_cast<std::size_t>(cu::c_atoi(*p.arg(1)));
    }
    if (!found) {
      return fail_text(codes::kUnknownStyle, "Unknown style " + *p.arg(1) +
                                                 "\nUse a number or one of the long names in the .dstyle file");
    }
    style.index = *found;
  }
  const std::string text = p.primargs > 3 ? *p.arg(2) : std::string{"selection"};
  const db::CellDef* def = c.ed->selection.def();
  if (def == nullptr || c.ed->selection.root() == nullptr) {
    return CommandResult::success();
  }
  for (std::size_t pl = tech::kFirstUserPlane; pl < def->plane_count(); ++pl) {
    def->plane(static_cast<tech::PlaneId>(pl)).enumerate(db::whole_plane(), ~tech::TypeMask::of(tech::kSpace),
                                                         [&](const db::TileView& t) {
                                                           view::FeedbackStyle s = style;
                                                           if (t.split) {
                                                             s = view::tile_feedback_style(style.index, t);
                                                           }
                                                           (void)c.ed->feedback.add(c.ed->windows,
                                                                                    *c.ed->selection.root(), t.area,
                                                                                    1, s, text, &c.ed->highlights);
                                                           return true;
                                                         });
  }
  return CommandResult::success();
}

CommandResult utility(const Context& c, CommandContext& ctx, const Parsed& p) {
  const bool plain = !(p.more || p.less) && p.primargs <= 2;
  switch (p.option) {
    case kClear:
      if (!plain) {
        return usage_error(kClear);
      }
      {
        Snapshot before = ops::begin(c);
        c.ed->selection.clear();
        ops::commit(c, std::move(before));
      }
      return CommandResult::success();
    case kLabels:
      c.ed->selection.label_policy = p.policy;
      cu::print(c.s, p.policy != connect::LabelPolicy::none ? "Selection includes subcell labels"
                                                            : "Selection ignores subcell labels");
      return CommandResult::success();
    case kHelp:
      return help(c);
    case kBbox:
      return bbox(c);
    case kSave:
      return save(c, ctx, p);
    case kFeedback:
      return feedback(c, p);
    default:
      break;
  }
  return CommandResult::success();
}

CommandResult floating(const Context& c, const Parsed& p) {
  if (p.option == kMove) {
    return move_floating(c, p);
  }
  if (p.more || p.less || p.primargs > 2) {
    return usage_error(p.option);
  }
  Snapshot before = c.ed->selection.snapshot();
  switch (p.option) {
    case kKeep:
      ops::keep_selection(c);
      break;
    case kPick:
      ops::delete_selection(c, "picked", false);
      break;
    case kFlat:
      ops::select_flat(c);
      break;
    default:
      break;
  }
  ops::commit(c, std::move(before));
  return CommandResult::success();
}

// ---- point, box and cell selection --------------------------------------------

using Level = CycleState::Level;

[[nodiscard]] Level level_of(int option) {
  switch (option) {
    case kChunk:
      return Level::chunk;
    case kRegion:
      return Level::region;
    case kNet:
      return Level::net;
    default:
      return Level::none;
  }
}

/// The next candidate type after `from`, wrapping (CmdSelect's type loop):
/// user types first, the stacked types only when no user type matches.
std::optional<tech::TileType> next_type(int from, const tech::TypeMask& mask, const tech::Technology& tech) {
  const int first = tech::kFirstUserType;
  const int end = static_cast<int>(tech.user_type_end());
  int t = from;
  for (int count = 0; count <= end - first; ++count) {
    ++t;
    if (t >= end || t < first) {
      t = first;
    }
    if (mask.test(static_cast<tech::TileType>(t))) {
      return static_cast<tech::TileType>(t);
    }
  }
  for (std::size_t s = tech.user_type_end(); s < tech.type_count(); ++s) {
    if (mask.test(static_cast<tech::TileType>(s))) {
      return static_cast<tech::TileType>(s);
    }
  }
  return std::nullopt;
}

/// The reference area of a point, box or `at` selection.
struct Reference {
  db::Rect area;
  db::Point point;
  bool degenerate = false;  // the box corner: retry one unit down-left
};

[[nodiscard]] bool is_box_kind(int option) noexcept {
  return option == kBox || option == kChunk || option == kRegion || option == kNet;
}

/// The root area under the pointer (one unit at the point without pixels).
db::Rect pointer_area(const cmd::PointerState& pointer) {
  if (pointer.pixel_area.is_null()) {
    return db::Rect{pointer.point.x, pointer.point.y, pointer.point.x + 1, pointer.point.y + 1};
  }
  return pointer.pixel_area;
}

diag::Outcome<Reference> reference(const Context& c, const Parsed& p) {
  if (is_box_kind(p.option)) {
    const auto box = ops::root_box(c);
    if (!box) {
      return diag::make(codes::kNoBox, "Box tool must be present");
    }
    return Reference{db::Rect{box->x0, box->y0, box->x0 + 1, box->y0 + 1}, db::Point{box->x0, box->y0}, true};
  }
  if (p.at) {
    return Reference{db::Rect{p.at->x, p.at->y, p.at->x + 1, p.at->y + 1}, *p.at, false};
  }
  const auto& pointer = c.ed->command_point;
  if (!pointer && p.option == kCell && (p.primargs == 3 || p.dotop)) {
    return Reference{db::Rect{0, 0, 1, 1}, db::Point{}, false};  // a named or top cell needs no pointer
  }
  if (!pointer) {
    return diag::make(codes::kNoPointer, "Put the cursor in a layout window");
  }
  return Reference{pointer_area(*pointer), pointer->point, false};
}

/// CmdSelect recasts a selection made at the pointer as the same command
/// followed by `at Xi Yi` (the lower-left corner of the point's root area)
/// so that the command log no longer depends on the pointer.
void log_at_pointer(CommandResult& result, const Context& c, const Parsed& p, const Invocation& inv) {
  if (is_box_kind(p.option) || p.at || !c.ed->command_point) {
    return;
  }
  const db::Rect area = pointer_area(*c.ed->command_point);
  Invocation logged{inv.name, inv.args, inv.origin, std::nullopt};
  logged.args.insert(logged.args.end(),
                     {"at", std::to_string(area.x0) + "i", std::to_string(area.y0) + "i"});
  result.logged_as = std::move(logged);
}

/// Same-place detection (MARGIN 2) and the command counter.
bool at_same_place(const Context& c, Parsed& p, const Reference& ref) {
  CycleState& cy = c.ed->selection.cycle;
  const std::uint64_t now = c.ed->command_count;
  if (p.force_same && now > 0) {
    cy.last_command = now - 1;
  }
  // The counter advances in editor::post_command; a front end that does not
  // run it keeps it still, which also counts as "immediately following".
  const bool follows = now == cy.last_command + 1 || now == cy.last_command || p.force_same;
  bool same = p.same_place;
  if (!db::encloses(cy.last_area, ref.point) || !follows) {
    same = false;
    cy.last_use = nullptr;
  }
  cy.last_area = db::Rect{ref.point.x - 2, ref.point.y - 2, ref.point.x + 2, ref.point.y + 2};
  cy.last_command = now;
  return same;
}

/// Restricts the candidate types to the layer list given (layerspec).
diag::Outcome<tech::TypeMask> restrict_types(const Context& c, const Parsed& p, Reference& ref,
                                             tech::TypeMask mask) {
  auto layers = ops::parse_layers(c, *p.arg(1));
  if (!layers.ok()) {
    return diag::make(codes::kBadLayerSpec, "Invalid layer specification");
  }
  const tech::TypeMask wanted = layers.value().types;
  mask &= wanted;
  if (!mask.empty()) {
    return mask;
  }
  if (!ref.degenerate) {
    return diag::make(codes::kNoPaint, "No paint of this type under the cursor/box");
  }
  ref.area = db::apply(db::translation(-1, -1), ref.area);
  mask = ops::types_under(c, ref.area) & c.visible & ops::paint_types(*c.tech) & wanted;
  if (mask.empty()) {
    return diag::make(codes::kNoPaint, "No paint of this type under or next to the cursor/box");
  }
  return mask;
}

/// The level of this selection: the option's own level (choosing the next
/// type for region and net), or the next level when repeated at the same
/// place.
void choose_level(const Context& c, const Parsed& p, const tech::TypeMask& mask, bool same) {
  CycleState& cy = c.ed->selection.cycle;
  const Level asked = level_of(p.option);
  if (asked != Level::none) {
    cy.level = asked;
    const auto t = asked != Level::chunk ? next_type(cy.type, mask, *c.tech) : std::nullopt;
    if (t) {
      cy.type = *t;
    }
    return;
  }
  const bool advance = same && cy.less_cycle == p.less;
  cy.level = !advance                   ? Level::chunk
             : cy.level == Level::chunk  ? Level::region
             : cy.level == Level::region ? Level::net
                                         : Level::chunk;
  if (cy.level != Level::chunk && (cy.type < 0 || !mask.test(static_cast<tech::TileType>(cy.type)))) {
    cy.level = Level::chunk;
    c.ed->selection.clear();
  }
}

/// Chooses the level and type, then selects chunk / region / net.
CommandResult grow_selection(const Context& c, const Parsed& p, const Reference& ref, const tech::TypeMask& mask,
                             bool same) {
  CycleState& cy = c.ed->selection.cycle;
  choose_level(c, p, mask, same);
  cy.less_cycle = p.less;
  if (cy.level == Level::chunk) {
    const auto t = next_type(cy.type, mask, *c.tech);
    if (!t) {
      return CommandResult::success();
    }
    cy.type = *t;
    cy.chunk = ops::select_chunk(c, ref.area, *t, p.less);
    if (!p.less && !cy.chunk.is_null()) {
      ops::set_root_box(c, cy.chunk);
    }
  }
  const auto type = static_cast<tech::TileType>(cy.type);
  if (cy.level == Level::region && db::surround(cy.chunk, ops::select_region(c, ref.area, type, p.less))) {
    cy.level = Level::net;  // the region added nothing beyond the chunk
  }
  if (cy.level == Level::net) {
    (void)ops::select_net(c, ref.area, type, p.less);
  }
  return CommandResult::success();
}

/// DBSelectCell's cycling: the next use after the last one, the root after
/// the largest.
std::optional<ops::UseRef> next_cell(const Context& c, const Reference& ref, bool same, bool less) {
  CycleState& cy = c.ed->selection.cycle;
  if (cy.last_use == c.root || !same || cy.less_cell_cycle != less) {
    cy.last_use = nullptr;
  }
  cy.less_cell_cycle = less;
  const std::vector<ops::UseRef> candidates = ops::uses_at(c, ref.area);
  std::size_t next = 0;
  if (cy.last_use != nullptr) {
    for (std::size_t i = 0; i < candidates.size(); ++i) {
      if (candidates[i].use == cy.last_use && candidates[i].element == cy.last_indices) {
        next = i + 1;
      }
    }
  }
  if (next >= candidates.size()) {
    return std::nullopt;
  }
  return candidates[next];
}

/// The result of `select cell`: the instance path with `\[`/`\]` unescaped.
std::string unescaped(const std::string& path) {
  std::string clean;
  for (std::size_t i = 0; i < path.size(); ++i) {
    const bool escape = path[i] == '\\' && i + 1 < path.size() && (path[i + 1] == '[' || path[i + 1] == ']');
    if (!escape) {
      clean += path[i];
    }
  }
  return clean;
}

/// The chosen element's bounding box in root coordinates (the new box).
db::Rect element_box(const Context& c, const ops::UseRef& chosen) {
  if (chosen.use == nullptr) {
    return c.root->bbox();
  }
  const db::Transform et = db::element_transform(*chosen.use, chosen.element.x, chosen.element.y);
  return db::apply(db::compose(chosen.parent_to_root, et), chosen.use->def->bbox());
}

/// The use a cell selection picks (nullopt: the window's top cell).
diag::Outcome<std::optional<ops::UseRef>> pick_cell(const Context& c, const Parsed& p, const Reference& ref,
                                                    bool& same, bool& named) {
  const bool with_name = p.option == kCell && p.primargs == 3;
  if (with_name && !(p.isqual == 1 && !p.more && !p.less)) {
    named = true;
    same = false;
    c.ed->selection.cycle.last_area = db::Rect{-1000, -1000, -1000, -1000};
    if (p.arg(1)->empty()) {
      // DBTreeFindUse("") answers the window's root use (`select cell ""`,
      // what toolkit.tcl does after a failed getcell).
      return std::optional<ops::UseRef>{};
    }
    auto found = ops::find_use_path(c, *p.arg(1));
    if (!found) {
      return diag::make(codes::kNoSuchUse, "Couldn't find a cell use named \"" + *p.arg(1) + "\"");
    }
    return std::optional<ops::UseRef>{std::move(found)};
  }
  if (p.dotop || with_name) {
    return std::optional<ops::UseRef>{};
  }
  return next_cell(c, ref, same, p.less);
}

CommandResult cell_select(const Context& c, const Parsed& p, const Reference& ref, bool same, bool layerspec) {
  if (layerspec) {
    auto layers = ops::parse_layers(c, *p.arg(1));
    return fail_text(layers.ok() ? codes::kNoPaint : codes::kBadLayerSpec,
                     layers.ok() ? "No paint of this type under the cursor/box" : "Invalid layer specification");
  }
  bool named = false;
  auto picked = pick_cell(c, p, ref, same, named);
  if (!picked.ok()) {
    return cu::fail(picked.error());
  }
  const ops::UseRef chosen = picked.value().value_or(ops::UseRef{});
  CycleState& cy = c.ed->selection.cycle;
  cy.last_use = chosen.use != nullptr ? static_cast<const void*>(chosen.use) : static_cast<const void*>(c.root);
  cy.last_indices = chosen.element;
  if (p.less) {
    ops::unselect_cell(c, chosen);
  } else {
    ops::select_cell(c, chosen, same && !named);
  }
  ops::set_root_box(c, element_box(c, chosen));
  // A named selection prints the name as given (printPath = optionArgs[1]).
  const std::string printed = chosen.use != nullptr ? chosen.path : named ? *p.arg(1) : std::string{cell::kTopUseId};
  return CommandResult::success(Value{unescaped(printed)});
}

/// Paint selection at the reference (chunk / region / net), the layer list
/// applied first.
CommandResult paint_select(const Context& c, const Parsed& p, Reference& ref, tech::TypeMask mask, bool same,
                           bool layerspec) {
  if (layerspec) {
    auto restricted = restrict_types(c, p, ref, mask);
    if (!restricted.ok()) {
      return cu::fail(restricted.error());
    }
    mask = restricted.value();
  }
  // With magic-compat OFF a label-policy qualifier applies to this
  // command's region or net (Q-CMDEDIT-60); Magic uses the global policy.
  connect::LabelPolicy& policy = c.ed->selection.label_policy;
  const connect::LabelPolicy saved = policy;
  if (!c.s.magic_compat && p.policy_given) {
    policy = p.policy;
  }
  CommandResult result = grow_selection(c, p, ref, mask, same || level_of(p.option) != Level::none);
  policy = saved;
  return result;
}

CommandResult point_select(const Context& c, Parsed& p) {
  auto r = reference(c, p);
  if (!r.ok()) {
    return cu::fail(r.error());
  }
  Reference ref = r.value();
  Snapshot before = ops::begin(c);
  if (!(p.more || p.less)) {
    c.ed->selection.clear();
  }
  const tech::TypeMask mask = ops::types_under(c, ref.area) & c.visible & ops::paint_types(*c.tech);
  const bool same = at_same_place(c, p, ref);
  const bool layerspec = (p.option == kDefault && p.primargs == 2) ||
                         (p.option != kDefault && p.option != kCell && p.primargs == 3);
  CommandResult result = !mask.empty() && p.option != kCell ? paint_select(c, p, ref, mask, same, layerspec)
                                                             : cell_select(c, p, ref, same, layerspec);
  ops::commit(c, std::move(before));
  return result;
}

}  // namespace

CommandResult cmd_select(CommandContext& ctx, const Invocation& inv) {
  auto cx = ops::context(ctx);
  if (!cx.ok()) {
    return cu::fail(cx.error());
  }
  const Context& c = cx.value();
  Parsed p;
  p.argv.push_back(inv.name);
  p.argv.insert(p.argv.end(), inv.args.begin(), inv.args.end());
  if (p.argv.size() >= 2) {
    apply_qualifier(p, match_qualifier(p, c.s.magic_compat), c.ed->selection);
    if (auto st = take_at(p, c); !st.ok()) {
      return cu::fail(st.error());
    }
  }
  if (auto early = choose_option(p, c)) {
    return *early;
  }
  switch (p.option) {
    case kArea:
    case kVisible:
      return area_select(c, p);
    case kIntersection:
      return intersection(c, p);
    case kKeep:
    case kMove:
    case kPick:
    case kFlat:
      return floating(c, p);
    case kShort:
      if (p.primargs != 4) {
        return usage_error(kShort);
      }
      return select_short(c, *p.arg(1), *p.arg(2));
    case kBox:
    case kChunk:
    case kRegion:
    case kNet:
    case kDefault:
    case kCell: {
      CommandResult result = point_select(c, p);
      if (result.ok()) {
        log_at_pointer(result, c, p, inv);
      }
      return result;
    }
    default:
      return utility(c, ctx, p);
  }
}

}  // namespace hefesto::select
