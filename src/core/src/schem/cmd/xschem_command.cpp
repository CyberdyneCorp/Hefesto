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
// CORE-NOTIMPL-007, as the table says.
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

#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/not_implemented.hpp"
#include "hefesto/script/interp.hpp"
#include "internal.hpp"

namespace hefesto::schem {

namespace {

using namespace cmdimpl;
using Args = std::span<const std::string>;

constexpr std::array<std::string_view, 36> kSubcommands{
    "annotate_op", "change_sch_path", "descend",   "expandlabel",    "get",          "get_tok",
    "getprop",     "go_back",         "hilight_instname", "hilight_netname", "instance_list", "load",
    "load_new_window", "logic_set",   "netlist",   "place_symbol",   "raw",          "raw_clear",
    "raw_read",    "rebuild_connectivity", "redraw", "reset_caches", "save",         "search",
    "select",      "selected_set",    "set",       "setprop",        "simulate",     "subst_tok",
    "translate",   "unhilight_all",   "unselect_all", "update_all_sym_bboxes", "version", "zoom_full"};

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

cmd::CommandResult done(cmd::Value value = {}) {
  return cmd::CommandResult::success(std::move(value));
}

std::optional<long long> parse_int(std::string_view text) {
  long long value = 0;
  const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc{} || end != text.data() + text.size()) return std::nullopt;
  return value;
}

// Element `n` of the instance name `name` (1 the leftmost, -1 the
// rightmost, 0 the leftmost; other values count from that end); nullopt
// when out of range.
std::optional<std::string> vector_element(std::string_view name, long long n) {
  const std::vector<std::string> bits = expand_bus(name);
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
        {"descend", &Xschem::descend},        {"expandlabel", &Xschem::expandlabel},
        {"get", &Xschem::get},                {"get_tok", &Xschem::get_tok},
        {"getprop", &Xschem::getprop},        {"go_back", &Xschem::go_back},
        {"hilight_instname", &Xschem::hilight_instname}, {"hilight_netname", &Xschem::hilight_netname},
        {"instance_list", &Xschem::instance_list}, {"load", &Xschem::load},
        {"load_new_window", &Xschem::load_new_window}, {"logic_set", &Xschem::nothing},
        {"netlist", &Xschem::netlist},        {"place_symbol", &Xschem::place_symbol},
        {"raw", &Xschem::raw},                {"raw_clear", &Xschem::m9},
        {"raw_read", &Xschem::m9},            {"rebuild_connectivity", &Xschem::rebuild_connectivity},
        {"redraw", &Xschem::nothing},         {"reset_caches", &Xschem::reset_caches},
        {"save", &Xschem::save},              {"search", &Xschem::search},
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
  cmd::CommandResult place_symbol(Args /*args*/) {
    return fail(kXschemNeedsEditor, "xschem command 'place_symbol' needs the editor");
  }
  cmd::CommandResult version(Args args) { return args.empty() ? done(std::string{kWriterVersion}) : wrong_args("version"); }

  cmd::CommandResult raw(Args args) {
    if (args.size() == 1 && args[0] == "loaded") return done(-1);
    return m9(args);
  }

  // ---- property helpers ----------------------------------------------------
  cmd::CommandResult expandlabel(Args args) {
    if (args.size() != 1) return wrong_args("expandlabel");
    const BusExpansion bus = parse_bus(args[0]);
    return done(cmd::Value::List{join_bits(bus.bits, ","), static_cast<std::int64_t>(bus.width())});
  }

  cmd::CommandResult get_tok(Args args) {
    if (args.size() != 2) return wrong_args("get_tok");
    return done(get_token(args[0], args[1]).value_or(std::string{}));
  }

  cmd::CommandResult subst_tok(Args args) {
    if (args.size() != 3) return wrong_args("subst_tok");
    return done(set_token(args[0], args[1], std::string_view{args[2]}));
  }

  cmd::CommandResult translate(Args /*args*/) {
    return fail(not_implemented("xschem translate (text substitution tokens, SYMLIB)"));
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
    if (key == "current_name" || key == "schname" || key == "netlist_dir") return get_document_key(key);
    return unsupported("get " + key);
  }

  cmd::CommandResult get_document_key(const std::string& key) {
    const Document* doc = document();
    if (doc == nullptr) return no_document();
    if (key == "current_name") return done(doc->path.filename().generic_string());
    if (key == "schname") return done(doc->path.generic_string());
    return done(netlist_dir().generic_string());
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

  // The instance by name, else by index when the text is a number (xschem
  // accepts both).
  std::optional<std::size_t> instance_index(const Document& doc, const std::string& inst) {
    if (const auto found = find_instance(doc, inst)) return found;
    const auto n = parse_int(inst);
    if (n && *n >= 0 && static_cast<std::size_t>(*n) < doc.instances.size()) return static_cast<std::size_t>(*n);
    return std::nullopt;
  }

  cmd::CommandResult getprop_instance(Args args) {
    const Document* doc = document();
    if (doc == nullptr) return no_document();
    const auto index = instance_index(*doc, args[0]);
    if (!index) return fail(kNoObject, "schematic: no instance \"" + args[0] + "\"");
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
    return fail(kNoObject, "schematic: no box " + args[1] + " on layer " + args[0]);
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

  cmd::CommandResult netlist(Args args) {
    if (!args.empty()) return wrong_args("netlist");
    const Document* doc = document();
    if (doc == nullptr) return no_document();
    const std::string name = state_.netlist_name.empty() ? doc->path.stem().generic_string() + ".spice"
                                                         : std::exchange(state_.netlist_name, std::string{});
    const cmd::CommandResult result = schematic({"netlist", (netlist_dir() / name).generic_string()});
    return done(result.ok() ? 0 : 1);  // xschem returns 1 for a failed netlist, 0 for success
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
    if (expand_bus(inst).size() > 1) {
      const auto element = vector_element(inst, n);
      if (!element) return fail(kNoObject, "xschem descend: no element " + std::to_string(n) + " of " + inst);
      command.push_back(*element);
    }
    cmd::CommandResult result = schematic(std::move(command));
    if (result.ok()) state_.last_descended = inst;
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

void register_xschem_command(script::Interpreter& interpreter, Session& session) {
  auto xschem = std::make_shared<Xschem>(interpreter, session);
  interpreter.define_command("xschem", [xschem](const cmd::Invocation& inv) { return xschem->run(inv); });
}

}  // namespace hefesto::schem
