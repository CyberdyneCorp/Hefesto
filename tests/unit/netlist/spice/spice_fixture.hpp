// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixtures of the SPICE netlister tests: SpiceInput values built by hand
// from the NL-SPICE-* `.ext` inputs (tests/golden/NETLIST/cases), the
// `nltest` extraction style's device table, a command session, and a
// stand-in for EXTFLAT's SPICE name conversion (Magic's EFHNSprintf with
// magic-compat ON) until extflat::name_to_spice lands (tasks.md 10.3).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/netlist/spice/input.hpp"
#include "hefesto/netlist/spice/writer.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/ui/line.hpp"

namespace spice_test {

namespace spice = hefesto::netlist::spice;
namespace extflat = hefesto::extflat;

// EFHNSprintf / efHNSprintfPrefix (ext2spice.c:4308-4396) on components.
inline std::string convert_prefix(const std::string& part, const extflat::NameFlags& f) {
  std::string out;
  for (const char c : part) {
    if (f.convert_equal && c == '=') {
      out += ':';
    } else if (f.convert_brackets && (c == '[' || c == ']')) {
      out += '_';
    } else if (c == ',') {
      if (f.convert_comma) {
        out += '|';
      }
    } else {
      out += c;
    }
  }
  return out + "/";
}

inline std::string convert_leaf(const std::string& leaf, spice::Dialect format, const extflat::NameFlags& f) {
  if (!(f.trim_global || f.trim_local || f.convert_comma || f.convert_equal || f.convert_brackets)) {
    return leaf;
  }
  std::string out;
  for (const char c : leaf) {
    switch (c) {
      case '!':
        if (!f.trim_global) {
          out += c;
        }
        break;
      case '.':
        out += format == spice::Dialect::hspice ? '@' : '.';
        break;
      case '=':
        if (f.convert_equal) {
          out += ':';
        }
        break;
      case ',':
        if (f.convert_comma) {
          out += '|';
        }
        break;
      case '[':
      case ']':
        out += f.convert_brackets ? '_' : c;
        break;
      case '#':
        if (!f.trim_local) {
          out += c;
        }
        break;
      default:
        out += c;
    }
  }
  return out;
}

inline std::string magic_name(std::span<const std::string> parts, spice::Dialect format, extflat::NameFlags& f) {
  std::string out;
  for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
    out += convert_prefix(parts[i], f);
  }
  return parts.empty() ? out : out + convert_leaf(parts.back(), format, f);
}

inline const spice::NameConverter kMagicNames = magic_name;

inline std::vector<std::string> split(std::string_view path) {
  std::vector<std::string> parts;
  while (true) {
    const auto slash = path.find('/');
    parts.emplace_back(path.substr(0, slash));
    if (slash == std::string_view::npos) {
      return parts;
    }
    path.remove_prefix(slash + 1);
  }
}

// A node record: name, capacitance (fF, as the .ext file writes it with
// cscale 1000) and the area/perimeter pairs of the three resistance classes.
inline spice::InputNode node(const std::string& name, float cap_ff = 0.0F, std::vector<spice::AreaPerim> pa = {}) {
  spice::InputNode n;
  n.name = split(name);
  n.aliases = {name};
  n.cap = cap_ff * 1000.0F;
  pa.resize(3);
  n.pa = std::move(pa);
  return n;
}

inline spice::InputTerminal term(const std::string& node, std::int64_t length = 0, const char* attrs = nullptr) {
  spice::InputTerminal t;
  t.node = node;
  t.length = length;
  if (attrs != nullptr) {
    t.attributes = std::string{attrs};
  }
  return t;
}

// A device record: class, model, terminals, substrate and L/W.
inline spice::InputDevice device(spice::DeviceClass cls, const std::string& type,
                                 std::vector<spice::InputTerminal> terms, std::optional<std::string> substrate = {},
                                 std::int64_t l = 0, std::int64_t w = 0) {
  spice::InputDevice d;
  d.device_class = cls;
  d.type = type;
  d.terminals = std::move(terms);
  if (substrate) {
    d.substrate_local = *substrate;
    d.substrate = std::move(substrate);
  }
  d.length = l;
  d.width = w;
  return d;
}

inline spice::InputDevice mos(const std::string& type, std::int64_t l, std::int64_t w, const std::string& sub,
                              std::vector<spice::InputTerminal> terms) {
  return device(spice::DeviceClass::mosfet, type, std::move(terms), sub, l, w);
}

// The `nltest` style's ExtGetDevInfo table (authoring/nltest.tech): ndiff
// and pdiff are resistance class 0, poly 1, metal1 2.
inline std::vector<hefesto::extract::DeviceInfo> nltest_info() {
  return {
      {"nfet", 0, 0, 0, -1, "GND!"},  {"pfet", 0, 0, 0, -1, "VDD!"}, {"rppd", 0, 1, 1, -1, ""},
      {"mimcapm", 0, 2, 2, -1, ""}, {"dn", 0, 0, 0, -1, ""},       {"npn", 0, 0, 0, -1, ""},
  };
}

// An input with the nltest style applied (tech nltest, S_out 1, three
// resistance classes, substrate records absent as in the NL cases).
inline spice::SpiceInput nltest_input(const std::string& cell) {
  spice::SpiceInput in;
  in.cell = cell;
  in.tech = "nltest";
  in.scale = 1.0F;
  in.resist_classes = 3;
  in.compat_substrate = true;
  spice::apply_device_info(in, nltest_info());
  return in;
}

inline spice::SpiceSettings with_format(spice::Dialect format) {
  spice::SpiceSettings s;
  s.format = format;
  return s;
}

// write_flat with Magic's name conversion; `settings` start from the
// session defaults.
inline spice::WriteResult write(const spice::SpiceInput& in, const spice::SpiceSettings& settings = {},
                                bool magic_compat = true, spice::RunFlags flags = {}) {
  hefesto::compat::CompatSettings compat;
  compat.magic_compat = magic_compat;
  extflat::NameFlags names;
  return spice::write_flat(in, settings, flags, compat, names, kMagicNames);
}

inline std::string golden(std::string_view relative) {
  std::ifstream in{std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/golden/NETLIST/cases" / relative,
                   std::ios::binary};
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

// A command session with every command registered.
struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  hefesto::cmd::ScriptSession script{project};
  hefesto::cmd::CommandRegistry registry;
  hefesto::editor::EditorState state;
  hefesto::cmd::Dispatcher dispatcher{registry, services.view(), project, script};
  hefesto::diag::CollectingSink printed;

  Session() {
    REQUIRE(hefesto::cmd::register_all_commands(registry));
    dispatcher.set_editor(&state);
    dispatcher.add_sink(printed);
  }

  hefesto::cmd::CommandResult run(std::string_view line) {
    hefesto::cmd::CommandResult last = hefesto::cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(dispatcher, line, hefesto::cmd::Origin::script)) {
      last = std::move(r);
    }
    return last;
  }

  // The Tcl result of a command, as the NL cases' `probe` prints it.
  std::string result(std::string_view line) { return run(line).value.to_string(); }

  std::string take_printed() {
    std::string text;
    for (const auto& d : printed.take()) {
      text += d.text + "\n";
    }
    return text;
  }
};

}  // namespace spice_test
