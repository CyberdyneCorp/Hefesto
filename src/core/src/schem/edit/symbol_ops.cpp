// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `symbol make|pins|toschematic` (capability schematic-editing "Labels and
// ports"): a symbol made from the ports of the active schematic, pin boxes
// added to the active symbol from the ports of its schematic, and a
// schematic made from the pins of the active symbol.
//
// Layout of a made symbol (Hefesto's; the spec fixes only its content): a
// box outline on layer 4 from (-60,-20) to (60, 20 n), n the larger number
// of pins on one side; inputs on the left at x = -80, outputs and inouts on
// the right at x = 80, one every 20 units from y = 0, each a 5 x 5 pin box
// on layer 5 with a line to the outline. A made schematic has one port per
// pin at the pin's position, `ipin`, `opin` or `iopin` by its `dir`.
//
// `make` and `toschematic` write a new file and change no open document,
// so they are no undo step; `pins` edits the active symbol and is one.
// `symbol fromschematic` is not implemented (spec issue: the table gives no
// meaning that differs from `make`).
//
// Owner: EDIT.

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/file/save.hpp"
#include "hefesto/schem/file/transform.hpp"
#include "hefesto/schem/not_implemented.hpp"
#include "hefesto/schem/sym/types.hpp"
#include "internal.hpp"

namespace hefesto::schem::editimpl {

namespace {

constexpr double kPinPitch = 20.0;
constexpr double kOutline = 60.0;
constexpr double kPinX = 80.0;
constexpr double kHalfPin = 2.5;

struct Port {
  std::string name;
  std::string direction;  // in, out, inout
};

std::vector<Port> ports_of(EditContext& ec, const Document& doc) {
  std::vector<Port> ports;
  for (const InstanceRecord& instance : doc.instances) {
    const Symbol& symbol = ec.session().symbols.resolve(Reference{instance.symbol}, doc, ec.resolve_context());
    const std::string type = symbol_type(symbol);
    if (!is_port_type(type)) continue;
    const auto lab = get_token(instance.props, "lab");
    if (lab && !lab->empty()) ports.push_back({*lab, std::string{port_direction(type)}});
  }
  return ports;
}

// `value` written so that get_token reads it back unchanged ("Property
// strings"): `"` and `\` escaped, and quoted when it holds a separator
// (review of PR #101: a port name with a blank gave malformed properties).
std::string encoded(std::string_view value) {
  std::string out;
  for (const char c : value) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  if (value.find_first_of(" \t\n;") != std::string_view::npos) out = '"' + out + '"';
  return out;
}

BoxRecord pin_box(Point at, const Port& port) {
  return {5, {at.x - kHalfPin, at.y - kHalfPin}, {at.x + kHalfPin, at.y + kHalfPin},
          "name=" + encoded(port.name) + " dir=" + encoded(port.direction)};
}

Document make_symbol_document(const std::vector<Port>& ports) {
  Document sym;
  sym.kind = FileKind::symbol;
  sym.file_version = "1.3";
  sym.globals.k = "type=subcircuit\nformat=\"@name @pinlist @symname\"\ntemplate=\"name=x1\"";
  std::array<double, 2> next{0.0, 0.0};  // left, right
  for (const Port& port : ports) {
    const bool left = port.direction == "in";
    double& y = next[left ? 0 : 1];
    const double x = left ? -kPinX : kPinX;
    sym.boxes.push_back(pin_box({x, y}, port));
    sym.lines.push_back({4, {x, y}, {left ? -kOutline : kOutline, y}, {}});
    y += kPinPitch;
  }
  const double bottom = std::max({next[0], next[1], kPinPitch});
  sym.boxes.insert(sym.boxes.begin(), BoxRecord{4, {-kOutline, -kPinPitch}, {kOutline, bottom}, {}});
  sym.texts.push_back({"@symname", {-kOutline, -kPinPitch - 15.0}, 0, 0, 0.3, 0.3, {}});
  sym.texts.push_back({"@name", {kOutline, -kPinPitch - 15.0}, 0, 0, 0.2, 0.2, {}});
  return sym;
}

std::string_view port_symbol(std::string_view direction) {
  if (direction == "in") return "ipin.sym";
  if (direction == "out") return "opin.sym";
  return "iopin.sym";
}

Document make_schematic_document(const Document& symbol) {
  Document sch;
  sch.file_version = "1.3";
  for (const BoxRecord& box : symbol.boxes) {
    if (box.layer != 5) continue;
    const Pin pin = make_pin(box);
    const std::string direction = pin.direction.empty() ? std::string{"inout"} : pin.direction;
    sch.instances.push_back({std::string{port_symbol(direction)}, box_centre(box), 0, 0,
                             "name=p" + std::to_string(sch.instances.size() + 1) + " lab=" + encoded(pin.name)});
  }
  return sch;
}

// `make` and `toschematic` skip run_edit (they change no open document), so
// they check "Read-only libraries" here: no file is written into the PDK
// store or the system library, even with `-force`.
cmd::CommandResult write_new(EditContext& ec, Document document, const platform::Path& file, bool force) {
  platform::FileSystem& fs = ec.ctx().services().files;
  if (read_only_file(ec.ctx(), file)) {
    return fail(kReadOnly, "schematic: " + file.generic_string() +
                               " is read-only (PDK store or system library); save the document in a user folder "
                               "first");
  }
  if (!force && fs.exists(file)) {
    return fail(kExists, "schematic: " + file.generic_string() + " exists; use \"schematic symbol make -force\"");
  }
  SaveOptions options;
  options.overwrite_changed_file = true;
  const auto saved = save_document(fs, document, file, options);
  if (!saved) return fail(saved.error());
  ec.session().symbols.forget(file);
  return cmd::CommandResult::success(file.generic_string());
}

platform::Path sibling(const Document& doc, std::string_view extension) {
  platform::Path file = doc.path;
  file.replace_extension(extension);
  return file;
}

cmd::CommandResult need_file(const Document& doc, FileKind kind) {
  if (doc.kind != kind) {
    return fail(cmdimpl::kNoObject, kind == FileKind::schematic ? "schematic: the active document is not a schematic"
                                                                 : "schematic: the active document is not a symbol");
  }
  if (doc.path.empty()) return fail(cmdimpl::kNoObject, "schematic: the active document has no file name");
  return cmd::CommandResult::success();
}

cmd::CommandResult make(EditContext& ec, std::span<const std::string> rest) {
  const bool force = rest.size() == 1 && rest[0] == "-force";
  if (!rest.empty() && !force) return usage("symbol");
  if (auto bad = need_file(ec.doc(), FileKind::schematic); !bad.ok()) return bad;
  return write_new(ec, make_symbol_document(ports_of(ec, ec.doc())), sibling(ec.doc(), ".sym"), force);
}

cmd::CommandResult to_schematic(EditContext& ec, std::span<const std::string> rest) {
  if (!rest.empty()) return usage("symbol");
  if (auto bad = need_file(ec.doc(), FileKind::symbol); !bad.ok()) return bad;
  return write_new(ec, make_schematic_document(ec.doc()), sibling(ec.doc(), ".sch"), false);
}

// `symbol pins`: one pin box per port of `<name>.sch` not yet a pin.
cmd::CommandResult add_pins(EditContext& ec, std::span<const std::string> rest) {
  if (!rest.empty()) return usage("symbol");
  if (auto bad = need_file(ec.doc(), FileKind::symbol); !bad.ok()) return bad;
  const platform::Path file = sibling(ec.doc(), ".sch");
  ReadOptions options;  // as the document itself is read (hierarchy.cpp open_child)
  options.xschem_compat = ec.ctx().project().compat.xschem_compat;
  options.warnings = &ec.ctx().sink();
  auto loaded = load_document(ec.ctx().services().files, file, options);
  if (!loaded) return fail(kNoSchematic, "schematic: cannot open " + file.generic_string());
  std::vector<std::string> existing;
  for (const BoxRecord& box : ec.doc().boxes) {
    if (box.layer == 5) existing.push_back(make_pin(box).name);
  }
  double y = 0.0;
  for (const Port& port : ports_of(ec, loaded.value())) {
    if (std::find(existing.begin(), existing.end(), port.name) != existing.end()) continue;
    ec.doc().boxes.push_back(pin_box({0.0, y}, port));
    y += kPinPitch;
    ec.changed();
  }
  return cmd::CommandResult::success();
}

constexpr std::array<std::string_view, 4> kSymbolWhat{"make", "pins", "fromschematic", "toschematic"};

cmd::CommandResult symbol_view(EditContext& ec, std::span<const std::string> args) {
  if (args.empty()) return usage("symbol");
  const auto what = cmdimpl::match_keyword(args[0], kSymbolWhat);
  if (!what) return fail(what.error());
  if (what.value() == 0) return make(ec, args.subspan(1));
  if (what.value() == 3) return to_schematic(ec, args.subspan(1));
  return fail(not_implemented("schematic symbol fromschematic"));
}

}  // namespace

cmd::CommandResult op_symbol(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  const auto what = inv.args.size() >= 2 ? cmdimpl::match_keyword(inv.args[1], kSymbolWhat)
                                         : diag::Outcome<std::size_t>{std::size_t{0}};
  const bool pins = what && what.value() == 1;
  if (pins) {
    return run_edit(ctx, inv, [](EditContext& ec, std::span<const std::string> args) {
      return add_pins(ec, args.subspan(1));
    });
  }
  return run_view(ctx, inv, symbol_view);
}

}  // namespace hefesto::schem::editimpl
