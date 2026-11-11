// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Port of toolkit.tcl's magic::netlist_to_layout and its helpers
// move_forward_by_width, get_and_move_inst, create_new_pin and
// generate_layout_add (Magic ref: tcltk/toolkit.tcl:119-639). The netlist
// is read through the interpreter (`file exists`, `open`), so the sandbox's
// filesystem layer and the interpreter's current directory apply as in
// Magic; each device line becomes the same `magic::gencell ... -spice ...`
// script Magic evaluates.

#include "hefesto/pcell/framework/netlist.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <regex>
#include <set>

#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/script/interp.hpp"
#include "pcell/framework/toolkit.hpp"

namespace hefesto::pcell {

using detail::Toolkit;

namespace {

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

std::string upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return s;
}

std::int64_t to_int(const std::string& s) {
  try {
    return std::stoll(s);
  } catch (const std::exception&) {
    return 0;
  }
}

// Tcl's `split $line`: one element per white-space character.
std::vector<std::string> tcl_split(std::string_view line) {
  std::vector<std::string> out(1);
  for (const char c : line) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      out.emplace_back();
    } else {
      out.back() += c;
    }
  }
  return out;
}

const std::regex& element_line() {
  static const std::regex re{R"(^[xmcrldq]([^ \t]+)[ \t](.*)$)", std::regex::icase};
  return re;
}

// `array -list <option>` of the selected use (the first sublist with
// magic-compat off).
std::vector<std::string> array_values(Toolkit& tk, const char* option) {
  std::vector<std::string> values = tk.query_list({"array", "-list", option});
  if (values.size() == 1) {
    auto first = split_tcl_list(values.front());
    values = first.ok() ? std::move(first).value() : std::vector<std::string>{};
  }
  return values;
}

// ---- helpers of generate_layout_add ----------------------------------------------

// magic::move_forward_by_width
void move_forward_by_width(Toolkit& tk, const std::string& instance) {
  tk.engine({"select", "cell", instance});
  const detail::InternalUnits units{tk};
  const auto count = array_values(tk, "count");
  const auto pitch = array_values(tk, "pitch");
  const std::int64_t anum = count.size() > 1 ? to_int(count[1]) : 0;
  const std::int64_t xpitch = !pitch.empty() ? to_int(pitch[0]) : 0;
  const auto bbox = tk.query_list({"box", "values"});
  if (bbox.size() < 4) {
    return;
  }
  const std::int64_t posx = to_int(bbox[0]) + (to_int(bbox[2]) - to_int(bbox[0])) + xpitch * anum;
  tk.engine({"box", "position", std::to_string(posx), bbox[1]});
}

// magic::get_and_move_inst
void get_and_move_inst(Toolkit& tk, const std::string& cell, const std::string& instance, std::int64_t anum) {
  const std::string placed = tk.engine({"getcell", cell});
  tk.engine({"select", "cell", placed});
  if (placed.empty()) {
    return;
  }
  tk.engine({"identify", instance});
  if (anum > 1) {
    tk.engine({"array", "1", std::to_string(anum)});
  }
  const detail::InternalUnits units{tk};
  const auto bbox = tk.query_list({"box", "values"});
  if (bbox.size() >= 4) {
    tk.engine({"box", "position", bbox[2], bbox[1]});
  }
}

// magic::create_new_pin
void create_new_pin(Toolkit& tk, const std::string& pin, int port) {
  const auto saved = tk.query_list({"units"});
  tk.engine({"units", "microns"});
  tk.engine({"box", "size", "1", "1"});
  tk.engine({"paint", "m1"});
  tk.engine({"label", pin, "FreeSans", "1", "0", "0", "0", "c", "m1"});
  tk.engine({"port", "make", std::to_string(port)});
  tk.engine({"box", "move", "s", "2"});
  std::vector<std::string> restore{"units"};
  restore.insert(restore.end(), saved.begin(), saved.end());
  tk.engine(restore);
}

// Deletes the not-found placeholders among the children of `cell`.
void delete_missing_children(Toolkit& tk, const std::string& cell) {
  for (const auto& child : tk.query_list({"cellname", "list", "children", cell})) {
    const auto flags = tk.query_list({"cellname", "flags", child});
    if (std::find(flags.begin(), flags.end(), "not-found") == flags.end()) {
      continue;
    }
    for (const auto& inst : tk.query_list({"cellname", "list", "instances", child})) {
      tk.engine({"select", "cell", inst});
      tk.engine({"delete"});
    }
    tk.engine({"cellname", "delete", child, "-noprompt"});
  }
}

std::vector<std::string> existing_ports(Toolkit& tk) {
  std::vector<std::string> names;
  std::int64_t i = to_int(tk.query({"port", "first"}));
  std::set<std::int64_t> seen;
  while (i > -1 && seen.insert(i).second) {
    names.push_back(tk.query({"port", std::to_string(i), "name"}));
    const std::string next = tk.query({"port", std::to_string(i), "next"});
    i = next.empty() ? -1 : to_int(next);
  }
  return names;
}

// Pins of the netlist absent from the layout are added; ports of the
// layout absent from the netlist are removed.
void reconcile_pins(Toolkit& tk, const std::vector<std::string>& subpins) {
  std::vector<std::string> pins;
  for (const auto& p : subpins) {
    if (!p.empty()) {
      pins.push_back(p);
    }
  }
  const std::vector<std::string> existing = existing_ports(tk);
  const auto contains = [](const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
  };
  int port = 0;
  for (const auto& pin : pins) {
    if (!contains(existing, pin)) {
      create_new_pin(tk, pin, port++);
    }
  }
  for (const auto& pin : existing) {
    if (!contains(pins, pin)) {
      const std::string layer = tk.engine({"goto", pin});
      tk.engine({"select", "area", layer});
      tk.engine({"erase", "label"});
    }
  }
}

// ---- one device line -----------------------------------------------------------

struct DeviceLine {
  std::vector<std::string> pins;  // tokens up to the first name=value
  std::vector<std::pair<std::string, std::string>> parameters;
};

DeviceLine parse_device_line(Toolkit& tk, const std::string& comp) {
  static const std::regex kParam{R"(^[ \t]*[^= \t]+=[^=]+)", std::regex::icase};
  static const std::regex kToken{R"(^[ \t]*([^ \t]+)[ \t]*([\s\S]*)$)", std::regex::icase};
  static const std::regex kQuoted{R"(^([^= \t]+)='([^']+)'[ \t]*([\s\S]*))", std::regex::icase};
  static const std::regex kBraced{R"(^([^= \t]+)=\{([^}]+)\}[ \t]*([\s\S]*))", std::regex::icase};
  static const std::regex kPlain{R"(^([^= \t]+)=([^= \t]+)[ \t]*([\s\S]*))", std::regex::icase};
  DeviceLine d;
  std::string rest = comp;
  std::smatch m;
  while (!rest.empty() && !std::regex_search(rest, kParam)) {
    if (!std::regex_match(rest, m, kToken)) {
      rest.clear();
      break;
    }
    d.pins.push_back(m[1].str());
    rest = m[2].str();
  }
  while (!rest.empty()) {
    if (std::regex_match(rest, m, kQuoted) || std::regex_match(rest, m, kBraced)) {
      d.parameters.emplace_back(m[1].str(), "{" + m[2].str() + "}");
    } else if (std::regex_match(rest, m, kPlain)) {
      d.parameters.emplace_back(m[1].str(), m[2].str());
    } else {
      tk.print_error("Error parsing line \"" + comp + "\"");
      tk.print_error("at:  \"" + rest + "\"");
      break;
    }
    rest = m[3].str();
  }
  return d;
}

// `int($M)`, else `expr [string trim $M ']`, as toolkit.tcl computes the multiplier.
std::int64_t multiplier(Toolkit& tk, const DeviceLine& d) {
  std::int64_t mult = 1;
  for (const auto& [name, value] : d.parameters) {
    if (upper(name) != "M") {
      continue;
    }
    static_cast<void>(tk.interp().set_var("::magic::_netlist_m", cmd::Value{value}));
    const cmd::CommandResult r = tk.evaluate(
        "if {[catch {expr {int($::magic::_netlist_m)}} m]} {expr [string trim $::magic::_netlist_m ']} else {set m}");
    mult = r.ok() ? to_int(tcl_string(r.value)) : mult;
  }
  return mult;
}

// The device type as the library spells it (case-insensitive match of
// `<type>_defaults` among the namespace's procedures).
std::string library_device(Toolkit& tk, const std::string& library, std::string devtype) {
  const cmd::CommandResult r = tk.evaluate("namespace eval ::" + library + " {info procs}");
  auto procs = split_tcl_list(r.ok() ? tcl_string(r.value) : std::string{});
  const std::vector<std::string> all = procs.ok() ? std::move(procs).value() : std::vector<std::string>{};
  const std::string wanted = devtype + "_defaults";
  if (std::find(all.begin(), all.end(), wanted) != all.end()) {
    return devtype;
  }
  for (const auto& proc : all) {
    if (lower(proc) == lower(wanted)) {
      return proc.substr(0, proc.find('_'));
    }
  }
  return devtype;
}

std::string escape_brackets(const std::string& name) {
  std::string out;
  for (const char c : name) {
    if (c == '[' || c == ']') {
      out += '\\';
    }
    out += c;
  }
  return out;
}

struct LayoutContext {
  std::string library;  // or $PDKNAMESPACE
  std::vector<std::string> existing;      // instance names already in the cell
  std::vector<std::string> existing_cells;
};

void seed_device(Toolkit& tk, LayoutContext& lc, const std::string& comp) {
  DeviceLine d = parse_device_line(tk, comp);
  if (d.pins.size() < 2) {
    tk.print_error("Error:  No device type found in line \"" + comp + "\"");
    tk.print_error("Tokens found are: \"" + merge_tcl_list(d.pins) + "\"");
    return;
  }
  const std::string instance = d.pins.front();
  std::string devtype = d.pins.back();
  if (!devtype.empty() && (std::isdigit(static_cast<unsigned char>(devtype[0])) != 0 || devtype[0] == '.')) {
    const char kind = static_cast<char>(std::tolower(static_cast<unsigned char>(instance[0])));
    const std::map<char, std::string> ideal{{'c', "capacitor"}, {'r', "resistor"}, {'l', "inductor"}};
    if (const auto it = ideal.find(kind); it != ideal.end()) {
      d.parameters.emplace_back("value", devtype);
      devtype = it->second;
    }
  }
  const std::int64_t mult = multiplier(tk, d);
  devtype = library_device(tk, lc.library, devtype);
  std::string script = "magic::gencell " + lc.library + "::" + devtype + " " + escape_brackets(instance) + " -spice";
  for (const auto& [name, value] : d.parameters) {
    script += " " + lower(name) + " " + value;
  }
  const auto found = std::find(lc.existing.begin(), lc.existing.end(), instance);
  if (found != lc.existing.end()) {
    if (devtype == lc.existing_cells[static_cast<std::size_t>(found - lc.existing.begin())]) {
      return;  // toolkit.tcl compares the device type with the cell name
    }
    tk.engine({"pushbox"});
    tk.engine({"select", "cell", instance});
    tk.engine({"delete"});
    tk.engine({"popbox"});
  }
  if (!tk.evaluate(script).ok()) {
    get_and_move_inst(tk, devtype, instance, mult);
  } else {
    move_forward_by_width(tk, instance);
  }
}

// magic::generate_layout_add
void generate_layout_add(Toolkit& tk, const std::string& cell, const std::vector<std::string>& subpins,
                         const std::vector<std::string>& components, const std::string& library) {
  const detail::InternalUnits units{tk};
  tk.engine({"load", cell, "-quiet"});
  delete_missing_children(tk, cell);
  tk.engine({"box", "0", "0", "0", "0"});
  if (!subpins.empty()) {
    reconcile_pins(tk, subpins);
  }
  tk.engine({"box", "size", "0", "0"});
  const auto scale = tk.query({"cif", "scale", "out"});
  double out = 0.0;
  try {
    out = std::stod(scale);
  } catch (const std::exception&) {
    out = 0.0;
  }
  const double posy = out != 0.0 ? 3.0 / out : 0.0;
  const auto rounded = static_cast<std::int64_t>(posy < 0 ? std::ceil(posy - 0.5) : std::floor(posy + 0.5));
  tk.engine({"box", "position", "0", std::to_string(rounded)});
  tk.engine({"select", "top", "cell"});
  LayoutContext lc;
  lc.library = library.empty() ? tcl_string(tk.interp().get_var("::PDKNAMESPACE").value_or(cmd::Value{}))
                               : library;
  lc.existing = tk.query_list({"instance", "list", "children"});
  tk.print("Cells already in the layout of " + cell + ":");
  for (const auto& comp : lc.existing) {
    lc.existing_cells.push_back(tk.query({"instance", "list", "celldef", comp}));
    tk.print(comp + " (" + lc.existing_cells.back() + ")");
  }
  for (const auto& comp : components) {
    seed_device(tk, lc, comp);
  }
  tk.engine({"save", cell});
}

// ---- netlist_to_layout ------------------------------------------------------------

// The lines of the netlist: comment lines dropped, CDL *.PININFO kept as
// .PININFO, `+` continuations joined (toolkit.tcl:470-498).
std::vector<std::string> netlist_lines(const std::string& text, bool cdl) {
  std::vector<std::string> raw;
  std::string current;
  for (const char c : text) {
    if (c == '\n') {
      raw.push_back(current);
      current.clear();
    } else {
      current += c;
    }
  }
  if (!current.empty()) {
    raw.push_back(current);  // `gets` returns a last line without newline
  }
  std::vector<std::string> data;
  std::string last;
  for (std::string line : raw) {
    if (cdl && line.rfind("*.", 0) == 0 && lower(line.substr(2, 7)) == "pininfo") {
      line = line.substr(1);
    }
    if (!line.empty() && line[0] == '*') {
      continue;
    }
    if (!line.empty() && line[0] == '+') {
      if (line.back() != ' ') {
        last += ' ';
      }
      last += line.substr(1);
    } else {
      data.push_back(last);
      last = line;
    }
  }
  data.push_back(last);
  return data;
}

struct NetlistFile {
  std::string root;  // file rootname of the tail
  std::string tail;
  bool cdl = false;
  std::string text;
};

// Reads the netlist through the interpreter; nullopt after printing Magic's message.
std::optional<NetlistFile> read_netlist(Toolkit& tk, const std::string& path) {
  static_cast<void>(tk.interp().set_var("::magic::_netlist_file", cmd::Value{path}));
  const auto exists = tk.evaluate("file exists $::magic::_netlist_file");
  if (!exists.ok() || tcl_string(exists.value) != "1") {
    tk.print_error("No such file " + path);
    return std::nullopt;
  }
  NetlistFile f;
  f.root = tcl_string(tk.evaluate("file rootname [file tail $::magic::_netlist_file]").value);
  f.tail = tcl_string(tk.evaluate("file tail $::magic::_netlist_file").value);
  f.cdl = tcl_string(tk.evaluate("file extension $::magic::_netlist_file").value) == ".cdl";
  tk.print("Creating layout from " + f.tail);
  const auto text = tk.evaluate("set f [open $::magic::_netlist_file r]; set d [read $f]; close $f; set d");
  if (!text.ok()) {
    tk.print_error("Error:  Cannot open file \"" + path + "\" for reading.");
    return std::nullopt;
  }
  f.text = tcl_string(text.value);
  return f;
}

// Placeholders for every subcircuit; true for the ones that already have layout.
std::map<std::string, bool> prepare_subcircuits(Toolkit& tk, const std::vector<std::string>& lines,
                                                const std::string& top, const std::string& root) {
  std::map<std::string, bool> existing;
  for (const auto& line : lines) {
    const auto tokens = tcl_split(line);
    if (lower(tokens.front()) != ".subckt" || tokens.size() < 2) {
      continue;
    }
    const std::string& sub = tokens[1];
    if (sub == top || sub == root) {
      existing[sub] = false;
      tk.engine({"load", sub, "-silent"});
      continue;
    }
    tk.engine({"load", sub, "-fail", "-silent"});
    if (tk.query({"cellname", "list", "exists", sub}) == sub) {
      tk.print("Subcircuit " + sub + " successfully loaded.");
      existing[sub] = true;
      tk.engine({"select", "top", "cell"});
      tk.engine({"expand"});
    } else {
      tk.print("Subcircuit " + sub + " not found. Will generate.");
      existing[sub] = false;
      tk.engine({"load", sub, "-silent"});
    }
  }
  return existing;
}

struct Parser {
  Toolkit& tk;
  const std::map<std::string, bool>& existing;
  std::string library;
  bool in_sub = false;
  bool in_cmd = false;
  std::string sub;
  std::vector<std::string> pins;
  std::vector<std::string> components;
  std::vector<std::string> top;

  void end_subcircuit() {
    const auto it = existing.find(sub);
    if (it != existing.end() && !it->second) {
      tk.print("Cell " + sub + " is empty. Generating initial layout...");
      generate_layout_add(tk, sub, pins, components, library);
    } else {
      tk.print("Cell " + sub + " already contains layout. Skipping generation.");
    }
    sub.clear();
    pins.clear();
    components.clear();
    in_sub = false;
  }

  void outside(const std::string& line) {
    static const std::set<std::string> kIgnored{".global", ".ic", ".option", ".end"};
    const auto tokens = tcl_split(line);
    const std::string keyword = lower(tokens.front());
    if (kIgnored.count(keyword) != 0) {
      return;
    }
    if (keyword == ".command") {
      in_cmd = true;
    } else if (keyword == ".subckt") {
      sub = tokens.size() > 1 ? tokens[1] : std::string{};
      pins.assign(tokens.size() > 2 ? tokens.begin() + 2 : tokens.end(), tokens.end());
      in_sub = true;
    } else if (std::regex_match(line, element_line())) {
      top.push_back(line);
    }
  }

  void inside(const std::string& line) {
    static const std::regex kEnds{R"(^[ \t]*\.ends[\s\S]*)", std::regex::icase};
    if (std::regex_match(line, kEnds)) {
      end_subcircuit();
    } else if (std::regex_match(line, element_line())) {
      components.push_back(line);
    }  // testbench devices (i v b e) and other lines are ignored
  }

  void line(const std::string& text) {
    static const std::regex kEndc{R"(^[ \t]*\.endc[\s\S]*)", std::regex::icase};
    if (in_cmd) {
      in_cmd = !std::regex_match(text, kEndc);
    } else if (!in_sub) {
      outside(text);
    } else {
      inside(text);
    }
  }
};

void load_first_new_top(Toolkit& tk, const std::vector<std::string>& before) {
  for (const auto& cell : tk.query_list({"cellname", "list", "top"})) {
    if (std::find(before.begin(), before.end(), cell) == before.end()) {
      tk.engine({"load", cell});
      return;
    }
  }
}

}  // namespace

cmd::CommandResult netlist_to_layout(cmd::CommandContext& ctx, const platform::Path& netlist,
                                     std::string_view library) {
  auto opened = Toolkit::open(ctx);
  if (!opened.ok()) {
    return cmd::CommandResult::raised(opened.error());
  }
  Toolkit& tk = opened.value();
  const detail::DepthGuard depth{tk.state()};
  const auto file = read_netlist(tk, netlist.string());
  if (!file) {
    return cmd::CommandResult::success();
  }
  const std::vector<std::string> lines = netlist_lines(file->text, file->cdl);
  const std::vector<std::string> before = tk.query_list({"cellname", "list", "top"});
  const std::string current_top = tk.query({"cellname", "list", "window"});
  const auto existing = prepare_subcircuits(tk, lines, current_top, file->root);
  tk.engine({"load", current_top});
  Parser parser{tk, existing, std::string{library}, false, false, {}, {}, {}, {}};
  for (const auto& line : lines) {
    parser.line(line);
  }
  if (!parser.top.empty()) {
    tk.engine({"load", current_top});
    generate_layout_add(tk, file->root, {}, parser.top, std::string{library});
  } else {
    load_first_new_top(tk, before);
  }
  tk.engine({"select", "clear"});
  return cmd::CommandResult::success();
}

}  // namespace hefesto::pcell
