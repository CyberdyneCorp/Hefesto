// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The edit context, the one-undo-step wrapper of every editing command and
// the per-document undo history (capability schematic-editing "Document
// model", "Undo and redo", "Read-only libraries"; design.md D11).
//
// Undo. Each editing command makes one UndoRecord (the document before and
// after). It is kept in the document's own history (Session::edit), which
// `schematic undo`/`redo` walk, and recorded as one event on the session
// undo log, so the editor-wide `undo` reverts it too. Both act on the same
// records: undoing a record through the log first undoes the newer records
// of its document, and redoing it first redoes the older ones, so the
// history of every document stays a prefix of done records. Descending,
// ascending, selecting and highlighting record nothing, so the history of a
// document survives navigation ("Undo after ascending").
//
// Owner: EDIT.

#include <algorithm>
#include <charconv>
#include <memory>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/undo/undo_log.hpp"
#include "internal.hpp"

namespace hefesto::schem::editimpl {

namespace {

OpenDocument* find_document(Session& session, DocumentId id) {
  for (OpenDocument& open : session.documents) {
    if (open.id == id) return &open;
  }
  return nullptr;
}

// Drops selected objects that no longer exist (after an undo or a failed
// command); the selection itself is never undone.
void prune_selection(Session& session, DocumentId id) {
  const OpenDocument* open = find_document(session, id);
  auto it = session.edit.selections.find(id);
  if (it == session.edit.selections.end()) return;
  if (open == nullptr) {
    session.edit.selections.erase(it);
    return;
  }
  std::erase_if(it->second,
                [&](const ObjectRef& ref) { return ref.index >= object_count(open->document, ref.kind); });
}

bool undo_one(Session& session, DocumentHistory& history) {
  if (history.position == 0) return false;
  const UndoRecord& record = *history.records[--history.position];
  if (OpenDocument* open = find_document(session, record.document)) restore_document(open->document, record.before);
  prune_selection(session, record.document);
  return true;
}

bool redo_one(Session& session, DocumentHistory& history) {
  if (history.position == history.records.size()) return false;
  const UndoRecord& record = *history.records[history.position++];
  if (OpenDocument* open = find_document(session, record.document)) restore_document(open->document, record.after);
  prune_selection(session, record.document);
  return true;
}

// The index of `record` in its document's history; nullopt once dropped.
std::optional<std::size_t> history_index(const DocumentHistory& history, const UndoRecord* record) {
  const auto it = std::find_if(history.records.begin(), history.records.end(),
                               [record](const auto& entry) { return entry.get() == record; });
  if (it == history.records.end()) return std::nullopt;
  return static_cast<std::size_t>(it - history.records.begin());
}

// One editing command on the session undo log.
class SchematicStep final : public undo::Event {
 public:
  explicit SchematicStep(std::shared_ptr<UndoRecord> record) : record_(std::move(record)) {}

  void undo(editor::EditorState& editor) override {
    DocumentHistory& history = editor.schem.edit.histories[record_->document];
    const auto index = history_index(history, record_.get());
    while (index && history.position > *index) (void)undo_one(editor.schem, history);
  }
  void redo(editor::EditorState& editor) override {
    DocumentHistory& history = editor.schem.edit.histories[record_->document];
    const auto index = history_index(history, record_.get());
    while (index && history.position <= *index) (void)redo_one(editor.schem, history);
  }
  [[nodiscard]] std::string describe() const override {
    return "schematic edit of document " + std::to_string(record_->document);
  }

 private:
  std::shared_ptr<UndoRecord> record_;
};

bool under(const platform::Path& path, const platform::Path& root) {
  if (root.empty()) return false;
  const platform::Path p = path.lexically_normal();
  const platform::Path r = root.lexically_normal();
  auto pi = p.begin();
  for (auto ri = r.begin(); ri != r.end(); ++ri, ++pi) {
    if (ri->empty()) continue;  // a trailing separator
    if (pi == p.end() || *pi != *ri) return false;
  }
  return true;
}

OpenDocument* active(cmd::CommandContext& ctx) {
  return ctx.editor() == nullptr ? nullptr : active_document(ctx.editor()->schem);
}

std::span<const std::string> option_args(const cmd::Invocation& inv) {
  return inv.args.empty() ? std::span<const std::string>{} : std::span<const std::string>{inv.args}.subspan(1);
}

}  // namespace

// ---- context ---------------------------------------------------------------

ResolveContext EditContext::resolve_context() {
  return ResolveContext{session_.search_path, ctx_.services().files, &ctx_.sink(), ctx_.project().compat.xschem_compat};
}

const Symbol& EditContext::symbol(const InstanceRecord& instance) {
  return session_.symbols.resolve(Reference{instance.symbol}, open_.document, resolve_context());
}

bool read_only_file(const cmd::CommandContext& ctx, const platform::Path& path) {
  if (path.empty() || ctx.editor() == nullptr) return false;
  const std::string pdk_root = cmdimpl::sandbox_env(ctx, "PDK_ROOT");
  if (!pdk_root.empty() && under(path, platform::Path{pdk_root})) return true;
  const std::string cad_root = cmdimpl::sandbox_env(ctx, "CAD_ROOT");
  if (!cad_root.empty() && under(path, platform::Path{cad_root} / "assets" / "schematic")) return true;
  const auto& roots = ctx.editor()->script.sandbox.read_only_roots;
  return std::any_of(roots.begin(), roots.end(), [&path](const platform::Path& root) { return under(path, root); });
}

cmd::CommandResult run_view(cmd::CommandContext& ctx, const cmd::Invocation& inv, const EditBody& body) {
  OpenDocument* open = active(ctx);
  if (open == nullptr) return cmdimpl::no_document();
  EditContext ec(ctx, ctx.editor()->schem, *open);
  return body(ec, option_args(inv));
}

cmd::CommandResult run_edit(cmd::CommandContext& ctx, const cmd::Invocation& inv, const EditBody& body) {
  OpenDocument* open = active(ctx);
  if (open == nullptr) return cmdimpl::no_document();
  if (read_only_file(ctx, open->document.path)) {
    return fail(kReadOnly, "schematic: " + open->document.path.generic_string() +
                               " is read-only (PDK store or system library); use \"schematic save <file>\" to "
                               "save it in a user folder");
  }
  Session& session = ctx.editor()->schem;
  auto record = std::make_shared<UndoRecord>();
  record->document = open->id;
  record->before = open->document;
  EditContext ec(ctx, session, *open);
  cmd::CommandResult result = body(ec, option_args(inv));
  if (!result.ok()) {
    open->document = std::move(record->before);
    prune_selection(session, open->id);
    return result;
  }
  if (!ec.was_changed()) return result;
  open->document.modified = true;
  update_buses(ec);
  record->after = open->document;
  record_step(ctx, session, std::move(record));
  return result;
}

// ---- undo ------------------------------------------------------------------

void restore_document(Document& document, const Document& state) {
  Document restored = state;
  restored.path = document.path;
  restored.disk_digest = document.disk_digest;
  if (document.disk_digest != state.disk_digest || document.path != state.path) restored.modified = true;
  document = std::move(restored);
}

void record_step(cmd::CommandContext& ctx, Session& session, std::shared_ptr<UndoRecord> record) {
  DocumentHistory& history = session.edit.histories[record->document];
  history.records.resize(history.position);
  history.records.push_back(record);
  if (history.records.size() > kUndoDepth) history.records.erase(history.records.begin());
  history.position = history.records.size();
  undo::UndoLog& log = ctx.editor()->undo;
  if (log.recording()) log.record(std::make_unique<SchematicStep>(std::move(record)));
}

std::size_t undo_steps(Session& session, DocumentId id, std::size_t steps) {
  DocumentHistory& history = session.edit.histories[id];
  std::size_t done = 0;
  while (done < steps && undo_one(session, history)) ++done;
  return done;
}

std::size_t redo_steps(Session& session, DocumentId id, std::size_t steps) {
  DocumentHistory& history = session.edit.histories[id];
  std::size_t done = 0;
  while (done < steps && redo_one(session, history)) ++done;
  return done;
}

// ---- arguments -------------------------------------------------------------

std::optional<double> number(std::string_view text) {
  return cmdimpl::parse_number(text);
}

std::optional<std::size_t> index_value(std::string_view text) {
  std::size_t value = 0;
  const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc{} || end != text.data() + text.size()) return std::nullopt;
  return value;
}

std::optional<int> int_value(std::string_view text) {
  int value = 0;
  const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc{} || end != text.data() + text.size()) return std::nullopt;
  return value;
}

std::optional<std::vector<double>> numbers(std::span<const std::string> args, std::size_t first, std::size_t count) {
  if (first + count > args.size()) return std::nullopt;
  std::vector<double> values;
  values.reserve(count);
  for (std::size_t i = first; i < first + count; ++i) {
    const auto value = number(args[i]);
    if (!value) return std::nullopt;
    values.push_back(*value);
  }
  return values;
}

}  // namespace hefesto::schem::editimpl
