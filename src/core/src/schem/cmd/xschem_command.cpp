// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `xschem` Tcl compatibility command (capability schematic-command
// "xschem compatibility command"; Q-SCHEM-10). Pure reads of the active
// document (`get`, `getprop instance|rect`, the property helpers) work on
// the session directly; everything that changes a document, reads files or
// needs the editor runs `::magic::schematic` in the same interpreter, so it
// is dispatched, logged and undone like any other `schematic` command.
// Subcommands whose behaviour belongs to M9 (simulation results) answer
// CORE-NOTIMPL-007, as the table says. Bus expansions take their text from
// the session's budget (Q-SCHEM-41). The interpreter also gets the
// `abs_sym_path` and `rel_sym_path` procedures, resolved against the active
// document (Q-SCHEM-30).
//
// Owner: CMD.

#include <algorithm>
#include <array>
#include <charconv>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/lib/reference.hpp"
#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/not_implemented.hpp"
#include "hefesto/schem/sym/tokens.hpp"
#include "hefesto/script/interp.hpp"
#include "internal.hpp"

namespace hefesto::schem {

namespace {

using namespace cmdimpl;
using Args = std::span<const std::string>;

constexpr std::array<std::string_view, 41> kSubcommands{
    "annotate_op", "change_sch_path", "clear",     "copy",           "descend",      "expandlabel",
    "get",         "get_tok",         "getprop",   "go_back",        "hilight_instname", "hilight_netname",
    "instance",    "instance_list",   "load",      "load_new_window", "logic_set",   "netlist",
    "paste",       "place_symbol",    "raw",       "raw_clear",      "raw_read",     "rebuild_connectivity",
    "redraw",      "reset_caches",    "save",      "saveas",         "search",       "select",
    "selected_set", "set",            "setprop",   "simulate",       "subst_tok",    "translate",
    "unhilight_all", "unselect_all",  "update_all_sym_bboxes", "version", "zoom_full"};

// The process exit status of a run whose `xschem netlist` reported failure,
// as xschem's (Q-SCHEM-61; schematic-command "Headless use").
constexpr int kNetlistFailedStatus = 10;

// `xschem set` variables that only change xschem's drawing or GUI state.
constexpr std::array<std::string_view, 9> kGuiVariables{"cadgrid",   "cadsnap",           "draw_window",
                                                        "line_width", "no_draw",          "no_undo",
                                                        "rectcolor", "semaphore",         "show_hidden_texts"};

cmd::CommandResult unsupported(std::string_view what) {
  return fail(kXschemUnsupported, "xschem command '" + std::string{what} + "' is not supported");
}

cmd::CommandResult wrong_args(std::string_view sub) {
  return fail(kUsage, "xschem " + std::string{sub} + ": wrong number of arguments");
}

cmd::CommandResult bad_argument(std::string_view sub, std::string_view word) {
  return fail(kUsage, "xschem " + std::string{sub} + ": bad argument \"" + std::string{word} + "\"");
}

cmd::CommandResult needs_editor(std::string_view sub) {
  return fail(kXschemNeedsEditor, "xschem command '" + std::string{sub} + "' needs the editor");
}

cmd::CommandResult done(cmd::Value value = {}) {
  return cmd::CommandResult::success(std::move(value));
}

// `value` with the warnings of the run attached.
cmd::CommandResult done_with(cmd::Value value, std::vector<diag::Diagnostic> warnings) {
  cmd::CommandResult result = done(std::move(value));
  result.diagnostics = std::move(warnings);
  return result;
}

// The file services the session's last `schematic` command recorded; the
// local file system before any (no document is open then).
platform::FileSystem& session_files(Session& session) {
  static platform::LocalFileSystem local;
  return session.files != nullptr ? *session.files : local;
}

// The project's xschem-compat as the session's last `schematic` command
// recorded it; ON (the default) before any.
bool session_compat(const Session& session) {
  return session.compat == nullptr || session.compat->xschem_compat;
}

// The bus expansion of `name` under the session's budget, with its warning
// (SCHEM-NET-002 or -006) added to `warnings`.
BusExpansion session_bus(std::string_view name, Session& session, std::vector<diag::Diagnostic>& warnings) {
  BusExpansion bus = parse_bus(name, &session.bus_budget);
  if (auto warning = bus_diagnostic(bus, name)) warnings.push_back(std::move(*warning));
  return bus;
}

std::optional<long long> parse_int(std::string_view text) {
  long long value = 0;
  const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc{} || end != text.data() + text.size()) return std::nullopt;
  return value;
}

// Whether `text` addresses an instance by index: decimal digits only.
bool all_digits(std::string_view text) {
  return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

// Element `n` of the instance name `name` (1 the leftmost, -1 the
// rightmost, 0 the leftmost; other values count from that end); nullopt
// when out of range.
std::optional<std::string> vector_element(const std::vector<std::string>& bits, long long n) {
  const auto size = static_cast<long long>(bits.size());
  const long long index = n == 0 ? 0 : (n > 0 ? n - 1 : size + n);
  if (index < 0 || index >= size) return std::nullopt;
  return bits[static_cast<std::size_t>(index)];
}

// The first `instance <name>` entry of a `schematic selection` result.
std::optional<std::string> first_selected_instance(const cmd::Value& selection) {
  const auto* list = std::get_if<cmd::Value::List>(&selection.data());
  if (list == nullptr) return std::nullopt;
  for (const cmd::Value& entry : *list) {
    const std::string text = entry.to_string();
    if (text.rfind("instance ", 0) == 0) return text.substr(9);
  }
  return std::nullopt;
}

// `head` followed by `tail`.
std::vector<std::string> words(std::vector<std::string> head, Args tail) {
  head.insert(head.end(), tail.begin(), tail.end());
  return head;
}

struct XschemState {
  std::string netlist_name;    // `xschem set netlist_name`, used by the next `xschem netlist`
  std::string last_descended;  // the instance of the last `xschem descend`
};

class Xschem {
 public:
  Xschem(script::Interpreter& interp, Session& session) : interp_(interp), session_(session) {}

  cmd::CommandResult run(const cmd::Invocation& inv) {
    if (inv.args.empty()) return unsupported("");
    const std::string& sub = inv.args.front();
    const auto it = handlers().find(sub);
    if (it == handlers().end()) return unsupported(sub);
    return (this->*(it->second))(Args(inv.args).subspan(1));
  }

 private:
  using Method = cmd::CommandResult (Xschem::*)(Args);

  static const std::map<std::string_view, Method>& handlers() {
    static const std::map<std::string_view, Method> table{
        {"annotate_op", &Xschem::m9},         {"change_sch_path", &Xschem::change_sch_path},
        {"clear", &Xschem::clear},            {"copy", &Xschem::copy},
        {"descend", &Xschem::descend},        {"expandlabel", &Xschem::expandlabel},
        {"get", &Xschem::get},                {"get_tok", &Xschem::get_tok},
        {"getprop", &Xschem::getprop},        {"go_back", &Xschem::go_back},
        {"hilight_instname", &Xschem::hilight_instname}, {"hilight_netname", &Xschem::hilight_netname},
        {"instance", &Xschem::instance},      {"instance_list", &Xschem::instance_list},
        {"load", &Xschem::load},
        {"load_new_window", &Xschem::load_new_window}, {"logic_set", &Xschem::nothing},
        {"netlist", &Xschem::netlist},        {"paste", &Xschem::paste},
        {"place_symbol", &Xschem::place_symbol},
        {"raw", &Xschem::raw},                {"raw_clear", &Xschem::m9},
        {"raw_read", &Xschem::m9},            {"rebuild_connectivity", &Xschem::rebuild_connectivity},
        {"redraw", &Xschem::nothing},         {"reset_caches", &Xschem::reset_caches},
        {"save", &Xschem::save},              {"saveas", &Xschem::saveas},
        {"search", &Xschem::search},
        {"select", &Xschem::select},          {"selected_set", &Xschem::selected_set},
        {"set", &Xschem::set},                {"setprop", &Xschem::setprop},
        {"simulate", &Xschem::m9},            {"subst_tok", &Xschem::subst_tok},
        {"translate", &Xschem::translate},    {"unhilight_all", &Xschem::unhilight_all},
        {"unselect_all", &Xschem::unselect_all}, {"update_all_sym_bboxes", &Xschem::nothing},
        {"version", &Xschem::version},        {"zoom_full", &Xschem::nothing},
    };
    return table;
  }

  // Runs `::magic::schematic <words>` in the interpreter; a failure raises.
  cmd::CommandResult schematic(std::vector<std::string> words) {
    const cmd::Invocation inv{"::magic::schematic", std::move(words), cmd::Origin::script, std::nullopt};
    cmd::CommandResult result = interp_.evaluate(cmd::format_command_line(inv));
    if (!result.ok()) result.raises = true;
    return result;
  }

  Document* document() {
    OpenDocument* open = active_document(session_);
    return open == nullptr ? nullptr : &open->document;
  }

  // ---- no-ops, M9 and the editor -------------------------------------------
  cmd::CommandResult nothing(Args /*args*/) { return done(); }
  cmd::CommandResult m9(Args /*args*/) { return fail(not_implemented("xschem simulation results (M9)")); }
  cmd::CommandResult place_symbol(Args /*args*/) { return needs_editor("place_symbol"); }
  cmd::CommandResult version(Args args) { return args.empty() ? done(std::string{kWriterVersion}) : wrong_args("version"); }

  cmd::CommandResult raw(Args args) {
    if (args.size() == 1 && args[0] == "loaded") return done(-1);
    return m9(args);
  }

  // ---- property helpers ----------------------------------------------------
  cmd::CommandResult expandlabel(Args args) {
    if (args.size() != 1) return wrong_args("expandlabel");
    std::vector<diag::Diagnostic> warnings;
    const BusExpansion bus = session_bus(args[0], session_, warnings);
    return done_with(cmd::Value::List{join_bits(bus.bits, ","), static_cast<std::int64_t>(bus.width())},
                     std::move(warnings));
  }

  // `xschem get_tok <s> <key> [<n>]` (Q-SCHEM-62 (a)): `<n>` 1 returns the
  // value as stored (a quoted value keeps its quotes), any other integer
  // the decoded value, as without `<n>`; both modes.
  cmd::CommandResult get_tok(Args args) {
    if (args.size() < 2 || args.size() > 3) return wrong_args("get_tok");
    ValueForm form = ValueForm::decoded;
    if (args.size() == 3) {
      const auto n = parse_int(args[2]);
      if (!n) return bad_argument("get_tok", args[2]);
      if (*n == 1) form = ValueForm::raw;
    }
    return done(get_token(args[0], args[1], form).value_or(std::string{}));
  }

  cmd::CommandResult subst_tok(Args args) {
    if (args.size() != 3) return wrong_args("subst_tok");
    return done(set_token(args[0], args[1], std::string_view{args[2]}));
  }

  // `xschem translate <inst> <text>`: the text substituted for the instance
  // `<inst>` ("Text substitution tokens"), addressed as for getprop; `-1`
  // substitutes with no instance, which gives the text unchanged; any other
  // instance that does not exist raises "instance not found", in both modes
  // (schematic-command "Translate with and without an instance").
  cmd::CommandResult translate(Args args) {
    if (args.size() != 2) return wrong_args("translate");
    const Document* doc = document();
    if (doc == nullptr) return no_document();
    if (args[0] == "-1") return done(args[1]);
    const auto index = instance_index(*doc, args[0]);
    if (!index) return fail(kNoObject, "xschem translate: instance not found");
    const std::size_t i = *index;
    diag::CollectingSink warnings;
    const SymbolLookup lookup = symbol_lookup(*doc);
    TextTokenContext context;
    context.instance = &doc->instances[i];
    context.symbol = &session_.symbols.resolve(Reference{doc->instances[i].symbol}, *doc, lookup.context(warnings));
    context.index = i;
    context.schematic = doc->path;
    context.top_schematic = doc->path;
    context.warnings = &warnings;
    std::string text = substitute_text(args[1], context);
    return done_with(std::move(text), warnings.take());
  }

  // What a symbol lookup outside a `schematic` command reads: the session's
  // search path, and the project's file services and xschem-compat that the
  // last `schematic` command recorded, so a symbol cached here is read as
  // the project's netlist and `getprop symbol` read it.
  struct SymbolLookup {
    const SearchPath& search_path;
    platform::FileSystem& fs;
    bool xschem_compat;
    [[nodiscard]] ResolveContext context(diag::DiagnosticSink& sink) const {
      return ResolveContext{search_path, fs, &sink, xschem_compat};
    }
  };
  SymbolLookup symbol_lookup(const Document& /*doc*/) {
    return SymbolLookup{session_.search_path, session_files(session_), session_compat(session_)};
  }

  // ---- get / set -----------------------------------------------------------
  cmd::CommandResult get(Args args) {
    if (args.size() != 1) return wrong_args("get");
    const std::string& key = args[0];
    if (key == "netlist_type") return done("spice");
    if (key == "raw_level") return done(0);
    if (key == "topwindow") return done(".");
    if (key == "current_win_path") return done(".drw");
    if (key == "version") return done(std::string{kWriterVersion});
    if (key == "sch_path") return schematic({"hierarchy", "path"});
    static constexpr std::array<std::string_view, 5> kDocumentKeys{"current_name", "schname", "netlist_dir",
                                                                   "instances", "wires"};
    if (std::find(kDocumentKeys.begin(), kDocumentKeys.end(), key) != kDocumentKeys.end()) {
      return get_document_key(key);
    }
    return unsupported("get " + key);
  }

  // The keys read from the active document. A document never saved is
  // named `untitled.sch` (`untitled.sym`), as `xschem clear` names it.
  cmd::CommandResult get_document_key(const std::string& key) {
    const Document* doc = document();
    if (doc == nullptr) return no_document();
    if (key == "instances") return done(static_cast<std::int64_t>(doc->instances.size()));
    if (key == "wires") return done(static_cast<std::int64_t>(doc->wires.size()));
    if (key == "current_name") return done(current_name(*doc));
    if (key == "schname") return done(doc->path.generic_string());
    return done(netlist_dir().generic_string());
  }

  static std::string current_name(const Document& doc) {
    if (!doc.path.empty()) return doc.path.filename().generic_string();
    return doc.kind == FileKind::symbol ? "untitled.sym" : "untitled.sch";
  }

  platform::Path netlist_dir() {
    if (session_.xschemrc) {
      const auto it = session_.xschemrc->variables.find("netlist_dir");
      if (it != session_.xschemrc->variables.end() && !it->second.empty()) return platform::Path{it->second};
    }
    const Document* doc = document();
    return doc == nullptr ? platform::Path{} : doc->path.parent_path();
  }

  cmd::CommandResult set(Args args) {
    if (args.size() != 2) return wrong_args("set");
    const std::string& var = args[0];
    if (var == "netlist_type") {
      if (args[1] == "spice") return done();
      return fail(kXschemNetlistType, "xschem set netlist_type: only spice is supported");
    }
    if (var == "netlist_name") {
      state_.netlist_name = args[1];
      return done();
    }
    if (var == "schprop") return schematic({"setprop", "schematic", "S", args[1]});
    if (var == "schsymbolprop") return schematic({"setprop", "schematic", "K", args[1]});
    if (var == "header_text") return fail(not_implemented("xschem set header_text"));
    if (std::find(kGuiVariables.begin(), kGuiVariables.end(), var) != kGuiVariables.end()) return done();
    return unsupported("set " + var);
  }

  // ---- getprop / setprop ---------------------------------------------------
  cmd::CommandResult getprop(Args args) {
    if (args.empty()) return wrong_args("getprop");
    if (args[0] == "instance" && (args.size() == 2 || args.size() == 3)) return getprop_instance(args.subspan(1));
    if (args[0] == "rect" && (args.size() == 3 || args.size() == 4)) return getprop_rect(args.subspan(1));
    if (args[0] == "symbol" && (args.size() == 2 || args.size() == 3)) return schematic(words({"getprop"}, args));
    return wrong_args("getprop");
  }

  // `<inst>` of the xschem table: an index (from 0, in file order) when it
  // is all decimal digits, otherwise an instance name, so an instance named
  // `3` is reached only by its index ("Instance addressed by index").
  static std::optional<std::size_t> instance_index(const Document& doc, const std::string& inst) {
    if (!all_digits(inst)) return find_instance(doc, inst);
    const auto n = parse_int(inst);
    if (n && static_cast<std::size_t>(*n) < doc.instances.size()) return static_cast<std::size_t>(*n);
    return std::nullopt;
  }

  cmd::CommandResult getprop_instance(Args args) {
    const Document* doc = document();
    if (doc == nullptr) return no_document();
    const auto index = instance_index(*doc, args[0]);
    if (!index) return fail(kNoObject, "xschem getprop: instance not found:" + args[0]);
    const auto key = args.size() > 1 ? std::optional<std::string>{args[1]} : std::nullopt;
    return done(property_value(doc->instances[*index].props, key));
  }

  cmd::CommandResult getprop_rect(Args args) {
    const Document* doc = document();
    if (doc == nullptr) return no_document();
    const auto layer = parse_int(args[0]);
    const auto n = parse_int(args[1]);
    long long seen = 0;
    for (const BoxRecord& box : doc->boxes) {
      if (!layer || !n || box.layer != *layer || seen++ != *n) continue;
      const auto key = args.size() > 2 ? std::optional<std::string>{args[2]} : std::nullopt;
      return done(property_value(box.props, key));
    }
    return fail(kNoObject, "xschem getprop rect: no such box");
  }

  cmd::CommandResult setprop(Args args) {
    if (!args.empty() && args[0] == "rect") return fail(not_implemented("xschem setprop rect"));
    if (args.size() < 3 || args.size() > 4 || args[0] != "instance") return wrong_args("setprop");
    return schematic(words({"setprop"}, args));
  }

  // ---- documents and hierarchy ---------------------------------------------
  cmd::CommandResult load(Args args) {
    if (args.empty() || args.size() > 2) return wrong_args("load");
    return schematic({"open", args[0], "-window", "current"});
  }

  cmd::CommandResult load_new_window(Args args) {
    if (args.size() > 1) return wrong_args("load_new_window");
    if (args.empty() || args[0].empty()) return schematic({"new", "schematic"});
    return schematic({"open", args[0], "-window", "new"});
  }

  cmd::CommandResult save(Args args) {
    if (args.size() > 1) return wrong_args("save");
    const Document* doc = document();
    if (doc == nullptr) return no_document();
    if (!doc->modified) return done();
    const cmd::CommandResult saved = schematic({"save"});
    return saved.ok() ? done() : saved;
  }

  // `xschem netlist` (Q-SCHEM-61 (a)). With xschem-compat ON it returns
  // xschem's value, 0 on success and 1 after a run that reports failure,
  // without raising: the file is written anyway (as xschem writes it) and
  // the run's exit status becomes 10. With xschem-compat OFF a failure
  // raises, as `schematic netlist` does. An interrupted run raises in both.
  cmd::CommandResult netlist(Args args) {
    if (!args.empty()) return wrong_args("netlist");
    const Document* doc = document();
    if (doc == nullptr) return no_document();
    const std::string name = state_.netlist_name.empty() ? doc->path.stem().generic_string() + ".spice"
                                                         : std::exchange(state_.netlist_name, std::string{});
    const bool compat = session_compat(session_);
    std::vector<std::string> command{"netlist", (netlist_dir() / name).generic_string()};
    if (compat) command.emplace_back("-force");
    cmd::CommandResult result = schematic(std::move(command));
    if (result.ok()) return done(0);
    if (!compat || interrupted(result)) return result;
    if (session_.script != nullptr && session_.script->exit_status == 0) {
      session_.script->exit_status = kNetlistFailedStatus;
    }
    return done(1);
  }

  static bool interrupted(const cmd::CommandResult& result) {
    return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                       [](const diag::Diagnostic& d) { return d.code == kNetlistInterrupted; });
  }

  // `xschem saveas <file> [schematic|symbol]`: `schematic save <file>`
  // without asking; the kind word is accepted (the file's extension gives
  // the kind written). 1 when the file is written, 0 otherwise.
  cmd::CommandResult saveas(Args args) {
    if (args.empty()) return needs_editor("saveas");
    if (args.size() > 2) return wrong_args("saveas");
    if (document() == nullptr) return no_document();
    return done(schematic({"save", args[0], "-force"}).ok() ? 1 : 0);
  }

  // `xschem clear [force] [schematic|symbol]`: the active document becomes
  // a new empty unsaved one. Without `force` a modified document is saved
  // first with xschem-compat ON (as xschem does in a run without its GUI;
  // one never saved is dropped), and refused with xschem-compat OFF
  // ("clear without force"; Hefesto has no prompt in a script run).
  cmd::CommandResult clear(Args args) {
    const bool force = !args.empty() && args[0] == "force";
    const Args rest = args.subspan(force ? 1 : 0);
    if (rest.size() > 1) return wrong_args("clear");
    const std::string kind = !rest.empty() && rest[0] == "symbol" ? "symbol" : "schematic";
    if (document() != nullptr) {
      cmd::CommandResult closed = force ? done() : save_before_clear();
      if (closed.ok()) closed = schematic({"close", "-discard"});
      if (!closed.ok()) return closed;
    }
    const cmd::CommandResult created = schematic({"new", kind});
    return created.ok() ? done() : created;
  }

  // "clear without force": a modified active document is saved first with
  // xschem-compat ON (one never saved is dropped), refused with it OFF.
  cmd::CommandResult save_before_clear() {
    const Document& doc = *document();
    if (!doc.modified) return done();
    if (!session_compat(session_)) return fail(kUnsaved, "schematic: unsaved changes");
    return doc.path.empty() ? done() : schematic({"save"});
  }

  // `xschem instance <sym> <x> <y> <rot> <flip> [<props>]`: `schematic place`.
  cmd::CommandResult instance(Args args) {
    if (args.size() < 5 || args.size() > 6) return wrong_args("instance");
    std::vector<std::string> command{"place", args[0], args[1], args[2], "-rot", args[3], "-flip", args[4]};
    if (args.size() == 6) {
      command.emplace_back("-props");
      command.push_back(args[5]);
    }
    const cmd::CommandResult placed = schematic(std::move(command));
    return placed.ok() ? done() : placed;
  }

  // `xschem copy`: the selection to the clipboard (`schematic clipboard copy`).
  cmd::CommandResult copy(Args args) {
    if (!args.empty()) return wrong_args("copy");
    const cmd::CommandResult copied = schematic({"clipboard", "copy"});
    return copied.ok() ? done() : copied;
  }

  // `xschem paste <x> <y>`: `schematic paste`; without a point it places
  // interactively, which needs the editor.
  cmd::CommandResult paste(Args args) {
    if (args.empty()) return needs_editor("paste");
    if (args.size() != 2) return wrong_args("paste");
    const cmd::CommandResult pasted = schematic({"paste", args[0], args[1]});
    return pasted.ok() ? done() : pasted;
  }

  cmd::CommandResult go_back(Args args) { return args.empty() ? schematic({"ascend"}) : wrong_args("go_back"); }

  cmd::CommandResult descend(Args args) {
    if (args.size() > 3) return wrong_args("descend");
    const long long n = args.empty() ? 1 : parse_int(args[0]).value_or(1);
    std::optional<std::string> inst = args.size() == 3 ? std::optional<std::string>{args[2]} : std::nullopt;
    if (!inst) {
      const cmd::CommandResult selection = schematic({"selection"});
      if (!selection.ok()) return selection;
      inst = first_selected_instance(selection.value);
    }
    if (!inst) return fail(kNoObject, "xschem descend: no instance selected");
    return descend_into(*inst, n);
  }

  cmd::CommandResult descend_into(const std::string& inst, long long n) {
    std::vector<std::string> command{"descend", inst};
    std::vector<diag::Diagnostic> warnings;
    const BusExpansion bus = session_bus(inst, session_, warnings);
    if (bus.bits.size() > 1) {
      const auto element = vector_element(bus.bits, n);
      if (!element) return fail(kNoObject, "xschem descend: no element " + std::to_string(n) + " of " + inst);
      command.push_back(*element);
    }
    cmd::CommandResult result = schematic(std::move(command));
    if (result.ok()) state_.last_descended = inst;
    result.diagnostics.insert(result.diagnostics.begin(), warnings.begin(), warnings.end());
    return result;
  }

  cmd::CommandResult change_sch_path(Args args) {
    if (args.empty() || args.size() > 2) return wrong_args("change_sch_path");
    if (state_.last_descended.empty()) return fail(kNoObject, "xschem change_sch_path: no instance descended into");
    const auto n = parse_int(args[0]);
    if (!n) return wrong_args("change_sch_path");
    const cmd::CommandResult up = schematic({"ascend"});
    return up.ok() ? descend_into(state_.last_descended, *n) : up;
  }

  // ---- selection and highlighting ------------------------------------------
  cmd::CommandResult select(Args args) { return schematic(words({"select"}, args)); }

  cmd::CommandResult unselect_all(Args args) {
    if (args.size() > 1) return wrong_args("unselect_all");
    const cmd::CommandResult result = schematic({"select", "clear"});
    return result.ok() ? done() : result;
  }

  cmd::CommandResult selected_set(Args args) {
    if (!args.empty() && args[0] != "inst") return fail(not_implemented("xschem selected_set " + args[0]));
    const cmd::CommandResult selection = schematic({"selection"});
    if (!selection.ok()) return selection;
    cmd::Value::List names;
    const auto* list = std::get_if<cmd::Value::List>(&selection.value.data());
    for (std::size_t i = 0; list != nullptr && i < list->size(); ++i) {
      const std::string text = (*list)[i].to_string();
      if (text.rfind("instance ", 0) == 0) names.emplace_back(text.substr(9));
    }
    return done(std::move(names));
  }

  cmd::CommandResult search(Args /*args*/) { return fail(not_implemented("xschem search")); }

  cmd::CommandResult hilight_instname(Args args) {
    const bool fast = !args.empty() && args[0] == "-fast";
    if (args.size() != (fast ? 2U : 1U)) return wrong_args("hilight_instname");
    return schematic({"hilight", "instance", args.back()});
  }

  cmd::CommandResult hilight_netname(Args args) {
    if (args.size() != 1) return wrong_args("hilight_netname");
    return schematic({"hilight", "net", args[0]});
  }

  cmd::CommandResult unhilight_all(Args /*args*/) {
    const cmd::CommandResult result = schematic({"hilight", "clear", "all"});
    return result.ok() ? done() : result;
  }

  // ---- connectivity, caches and lists ---------------------------------------
  cmd::CommandResult rebuild_connectivity(Args /*args*/) {
    (void)schematic({"check"});  // its list and its errors are not returned
    return done();
  }

  cmd::CommandResult reset_caches(Args /*args*/) {
    const cmd::CommandResult result = schematic({"reload_symbols"});
    return result.ok() ? done() : result;
  }

  // `{name symbol type}` per instance, in file order.
  cmd::CommandResult instance_list(Args args) {
    if (!args.empty()) return wrong_args("instance_list");
    const Document* doc = document();
    if (doc == nullptr) return no_document();
    std::map<std::string, std::string> types;
    cmd::Value::List list;
    for (const InstanceRecord& instance : doc->instances) {
      auto [it, added] = types.try_emplace(instance.symbol);
      if (added) it->second = symbol_type(instance.symbol);
      list.emplace_back(cmd::Value::List{get_token(instance.props, "name").value_or(std::string{}), instance.symbol,
                                         it->second});
    }
    return done(std::move(list));
  }

  std::string symbol_type(const std::string& reference) {
    const cmd::CommandResult type = schematic({"getprop", "symbol", reference, "type"});
    return type.ok() ? type.value.to_string() : std::string{};
  }

  script::Interpreter& interp_;
  Session& session_;
  XschemState state_;
};

}  // namespace

std::span<const std::string_view> xschem_subcommands() {
  return kSubcommands;
}

namespace {

// The folder `./`, `../` and `.` resolve against: the active document's;
// empty for an unsaved document or none, which resolves no relative
// reference (Q-SCHEM-30, Q-SCHEM-38 (a): never the folder Hefesto was
// started in, as the symbol cache does).
platform::Path reference_folder(Session& session) {
  const OpenDocument* active = active_document(session);
  if (active != nullptr && !active->document.path.empty()) return active->document.path.parent_path();
  return {};
}

// `abs_sym_path <ref>` ("tcleval values", table "Procedures that xschem
// provides"): the file `ref` resolves to by "Symbol references", whether or
// not it exists, with SCHEM-LIB-001 for a reference resolved against the
// schematic folder (Q-SCHEM-30); with no document folder a reference that
// needs one is returned unchanged.
cmd::CommandResult abs_sym_path(Session& session, const cmd::Invocation& inv) {
  if (inv.args.size() != 1) return fail(kUsage, "wrong # args: should be \"abs_sym_path reference\"");
  const ResolvedReference found =
      resolve_reference(inv.args[0], reference_folder(session), session.search_path, session_files(session));
  cmd::CommandResult result = done(found.file.is_absolute() ? found.file.generic_string() : inv.args[0]);
  if (found.via == ResolvedVia::relative || found.via == ResolvedVia::document_folder) {
    result.diagnostics.push_back(diag::make("SCHEM-LIB-001", "Relative symbol reference " + inv.args[0] +
                                                                 " resolved against the schematic folder " +
                                                                 found.folder.generic_string() +
                                                                 "; xschem resolves it against its start folder"));
  }
  return result;
}

// `rel_sym_path <file>`: the reference Hefesto writes for `file` (relative
// to the first search-path folder holding it, in its alias form, else the
// bare name in the document folder, else the path itself).
cmd::CommandResult rel_sym_path(Session& session, const cmd::Invocation& inv) {
  if (inv.args.size() != 1) return fail(kUsage, "wrong # args: should be \"rel_sym_path file\"");
  const platform::Path file{inv.args[0]};
  if (!file.is_absolute()) return done(inv.args[0]);
  return done(shortest_reference(file.lexically_normal(), reference_folder(session), session.search_path));
}

}  // namespace

void register_xschem_command(script::Interpreter& interpreter, Session& session) {
  auto xschem = std::make_shared<Xschem>(interpreter, session);
  interpreter.define_command("xschem", [xschem](const cmd::Invocation& inv) { return xschem->run(inv); });
  Session* s = &session;
  interpreter.define_command("abs_sym_path", [s](const cmd::Invocation& inv) { return abs_sym_path(*s, inv); });
  interpreter.define_command("rel_sym_path", [s](const cmd::Invocation& inv) { return rel_sym_path(*s, inv); });
}

}  // namespace hefesto::schem
