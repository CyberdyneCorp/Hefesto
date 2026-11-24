// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the editing operations (capability schematic-editing;
// design.md D11): the edit context, the one-undo-step wrapper, object
// geometry and the connectivity of one schematic.
//
// Owner: EDIT.

#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "../cmd/internal.hpp"
#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/schem/edit/edit_state.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/net/connectivity.hpp"
#include "hefesto/schem/session.hpp"
#include "hefesto/schem/sym/symbol.hpp"

namespace hefesto::schem::editimpl {

// ---- codes (codes.inc) -----------------------------------------------------
inline constexpr std::string_view kReadOnly = "SCHEM-EDIT-001";
inline constexpr std::string_view kNoDescend = "SCHEM-EDIT-002";
inline constexpr std::string_view kDepthLimit = "SCHEM-EDIT-003";
inline constexpr std::string_view kAtTop = "SCHEM-EDIT-004";
inline constexpr std::string_view kWidthMismatch = "SCHEM-EDIT-005";
inline constexpr std::string_view kNoSchematic = "SCHEM-EDIT-006";
inline constexpr std::string_view kExists = "SCHEM-EDIT-007";

using cmdimpl::fail;
using cmdimpl::usage;

// ---- context ---------------------------------------------------------------

/// What an editing handler works on: the command context, the session and
/// the active document.
class EditContext {
 public:
  EditContext(cmd::CommandContext& ctx, Session& session, OpenDocument& open) noexcept
      : ctx_(ctx), session_(session), open_(open) {}

  [[nodiscard]] cmd::CommandContext& ctx() noexcept { return ctx_; }
  [[nodiscard]] Session& session() noexcept { return session_; }
  [[nodiscard]] Document& doc() noexcept { return open_.document; }
  [[nodiscard]] DocumentId id() const noexcept { return open_.id; }
  [[nodiscard]] Selection& selection() { return session_.edit.selections[open_.id]; }
  [[nodiscard]] ResolveContext resolve_context();
  /// The symbol of `instance` (the session cache; the placeholder when it
  /// cannot be resolved).
  [[nodiscard]] const Symbol& symbol(const InstanceRecord& instance);
  /// Marks the document changed by this command.
  void changed() noexcept { changed_ = true; }
  [[nodiscard]] bool was_changed() const noexcept { return changed_; }

 private:
  cmd::CommandContext& ctx_;
  Session& session_;
  OpenDocument& open_;
  bool changed_ = false;
};

using EditBody = std::function<cmd::CommandResult(EditContext&, std::span<const std::string>)>;

/// Runs `body` as one editing command on the active document: refused on a
/// read-only document (SCHEM-EDIT-001, "Read-only libraries"); on failure
/// the document is restored ("Return values and errors in scripts"); when
/// the body reported a change, the document is marked modified, the bus
/// flags are updated ("Bus editing") and one undo step is recorded ("Undo
/// and redo").
[[nodiscard]] cmd::CommandResult run_edit(cmd::CommandContext& ctx, const cmd::Invocation& inv, const EditBody& body);

/// Runs `body` on the active document without an undo step or the
/// read-only check (queries, selection, navigation).
[[nodiscard]] cmd::CommandResult run_view(cmd::CommandContext& ctx, const cmd::Invocation& inv, const EditBody& body);

/// True when `path` is in the PDK store or the system library
/// ("Read-only libraries").
[[nodiscard]] bool read_only_file(const cmd::CommandContext& ctx, const platform::Path& path);

// ---- undo ------------------------------------------------------------------

/// Adds `record` to its document's history (discarding the undone records
/// and the oldest beyond kUndoDepth) and to the session undo log.
void record_step(cmd::CommandContext& ctx, Session& session, std::shared_ptr<UndoRecord> record);

/// Undoes or redoes up to `steps` records of document `id`; the number done.
std::size_t undo_steps(Session& session, DocumentId id, std::size_t steps);
std::size_t redo_steps(Session& session, DocumentId id, std::size_t steps);

/// `document` set to the state `state`, keeping where it lives on disk
/// (path and digest); modified when the file changed since `state`.
void restore_document(Document& document, const Document& state);

// ---- arguments -------------------------------------------------------------

[[nodiscard]] std::optional<double> number(std::string_view text);
[[nodiscard]] std::optional<std::size_t> index_value(std::string_view text);
[[nodiscard]] std::optional<int> int_value(std::string_view text);
/// Parses `count` coordinates from `args` starting at `first`.
[[nodiscard]] std::optional<std::vector<double>> numbers(std::span<const std::string> args, std::size_t first,
                                                         std::size_t count);

// ---- geometry --------------------------------------------------------------

/// An axis-aligned rectangle; `empty` until a point is added.
struct Rect {
  Point min;
  Point max;
  bool empty = true;

  void add(Point p) noexcept;
  void add(const Rect& r) noexcept;
  [[nodiscard]] bool contains(const Rect& r) const noexcept;
  [[nodiscard]] bool overlaps(const Rect& r) const noexcept;
  [[nodiscard]] bool contains(Point p) const noexcept;
};

[[nodiscard]] Rect rect_of(Point a, Point b) noexcept;

/// A rigid motion applied to selected objects.
struct Motion {
  enum class Kind : std::uint8_t { translate, rotate, flip };
  Kind kind = Kind::translate;
  Point delta;  // translate
  Point pivot;  // rotate and flip
};

[[nodiscard]] Point apply(const Motion& motion, Point p) noexcept;

/// Applies `motion` to object `ref` of `doc` (instances and texts change
/// their rotation and flip as well, arcs their start angle).
void apply(const Motion& motion, Document& doc, ObjectRef ref);

/// The anchor of an object for in-place rotation and flip: the instance
/// origin, the text anchor, the first point of a wire, line or polygon,
/// the first corner of a box, the centre of an arc.
[[nodiscard]] Point anchor(const Document& doc, ObjectRef ref);

/// Every coordinate of `ref` rounded to the nearest multiple of `snap`.
void align(Document& doc, ObjectRef ref, double snap);

/// The bounding box of `ref`; instances use the graphics of `symbol`
/// (their origin when it has none), texts their anchor.
[[nodiscard]] Rect bounds(const Document& doc, ObjectRef ref, const Symbol* symbol);

/// Whether `ref` has a part inside `area` (the "select touched" test).
[[nodiscard]] bool touches_area(const Document& doc, ObjectRef ref, const Rect& area, const Symbol* symbol);

/// The number of objects of `kind` in `doc`.
[[nodiscard]] std::size_t object_count(const Document& doc, ObjectKind kind) noexcept;

/// The pin points of `instance` placed with `symbol`, in symbol pin order.
[[nodiscard]] std::vector<Point> pin_points(const InstanceRecord& instance, const Symbol& symbol);

// ---- connectivity ----------------------------------------------------------

/// The connectivity of the active document of `ec` as the netlister sees it
/// (schem/net/connectivity.hpp) with its net names.
struct DocumentNets {
  SchematicNets nets;
  NetNaming naming;
};

[[nodiscard]] DocumentNets document_nets(EditContext& ec);

/// The connectivity of open document `open` of the session of `ec`.
[[nodiscard]] DocumentNets document_nets(EditContext& ec, OpenDocument& open);

/// "Bus editing": sets `bus=1` on wires whose net is more than one bit wide
/// and removes it from the others; reports new width mismatches.
void update_buses(EditContext& ec);

/// The hierarchy path of the active document (empty when the descent stack
/// does not lead to it).
[[nodiscard]] HierPath hierarchy_path(Session& session);

/// The selection of the active document as `schematic selection` returns
/// it: one `{<kind> <index-or-name>}` per object.
[[nodiscard]] cmd::Value selected_objects(EditContext& ec);

// ---- the operations ----------------------------------------------------------

cmd::CommandResult op_place(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_wire(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_line(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_box(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_arc(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_polygon(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_text(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_label(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_select(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_selection(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_delete(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_move(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_copy(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_rotate(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_flip(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_flipv(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_align(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_setprop(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_replace(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_undo(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_redo(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_descend(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_ascend(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_hierarchy(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_hilight(cmd::CommandContext&, const cmd::Invocation&);
cmd::CommandResult op_symbol(cmd::CommandContext&, const cmd::Invocation&);

}  // namespace hefesto::schem::editimpl
