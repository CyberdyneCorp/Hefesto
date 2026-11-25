// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared fixture of the lvs/read tests: a database, a collecting sink, an
// in-memory file system and a structural dump for comparisons.
//
// Owner: LVSREAD.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/read/reader.hpp"
#include "hefesto/platform/file_system.hpp"

namespace lvs_test {

namespace lvs = hefesto::lvs;
namespace diag = hefesto::diag;

struct Reading {
  lvs::NetlistDb db;
  diag::CollectingSink sink;
  hefesto::platform::MemoryFileSystem files;
  lvs::ReadOptions options;

  Reading() {
    options.sink = &sink;
    options.files = &files;
  }

  diag::Outcome<lvs::CellId> read(std::string_view text, std::string name = "in.spice") {
    return lvs::read_netlist(db, lvs::Source{std::move(name), std::string{text}}, lvs::Format::spice, options);
  }

  diag::Outcome<lvs::CellId> read_file(std::string name) {
    return lvs::read_netlist(db, lvs::Source{std::move(name), std::nullopt}, lvs::Format::spice, options);
  }

  void add_file(const std::string& path, std::string_view text) {
    REQUIRE(files.write_atomic(path, text).ok());
  }

  const lvs::Cell& cell(std::string_view name, lvs::FileNumber file = 0) const {
    const auto id = db.find_cell(name, file);
    REQUIRE(id.has_value());
    return db.cells.at(*id);
  }

  [[nodiscard]] bool has_cell(std::string_view name, lvs::FileNumber file = 0) const {
    return db.find_cell(name, file).has_value();
  }

  /// The texts of the collected diagnostics with `code`.
  [[nodiscard]] std::vector<std::string> texts(std::string_view code) const {
    std::vector<std::string> result;
    for (const auto& d : sink.snapshot()) {
      if (d.code == code) {
        result.push_back(d.text);
      }
    }
    return result;
  }

  [[nodiscard]] bool printed(std::string_view text) const {
    for (const auto& d : sink.snapshot()) {
      if (d.text == text) {
        return true;
      }
    }
    return false;
  }
};

inline const lvs::Instance& instance(const lvs::Cell& cell, std::string_view name) {
  for (const auto& i : cell.instances) {
    if (i.name == name) {
      return i;
    }
  }
  FAIL("no instance " << name << " in " << cell.name);
  return cell.instances.front();
}

/// The names of the nets an instance connects, in port order.
inline std::vector<std::string> connections(const lvs::Cell& cell, const lvs::Instance& i) {
  std::vector<std::string> names;
  for (const auto net : i.connections) {
    names.push_back(net == lvs::kNoNet ? std::string{"-"} : cell.net(net).name);
  }
  return names;
}

inline std::vector<std::string> port_names(const lvs::Cell& cell) {
  std::vector<std::string> names;
  for (const auto& port : cell.ports) {
    names.push_back(port.name);
  }
  return names;
}

inline std::string property(const lvs::Instance& i, std::string_view key) {
  REQUIRE_FALSE(i.properties.empty());
  const auto* p = lvs::find_property(i.properties.front(), key);
  return p == nullptr ? std::string{"<none>"} : p->value;
}

/// A text form of the whole database, for equality checks.
inline std::string dump(const lvs::NetlistDb& db) {
  std::string out;
  for (const auto& cell : db.cells) {
    out += cell.name + "/" + std::to_string(cell.file) + " class=" + std::string{lvs::device_class_name(cell.device_class)};
    out += cell.flags.top ? " top" : "";
    out += cell.flags.placeholder ? " placeholder" : "";
    out += cell.flags.duplicate ? " duplicate" : "";
    out += "\n ports:";
    for (const auto& port : cell.ports) {
      out += " " + port.name + "=" + std::to_string(port.net);
    }
    out += "\n nets:";
    for (const auto& net : cell.nets) {
      out += " " + net.name + (net.global ? "(g)" : "");
    }
    for (const auto& property : cell.properties) {
      out += "\n prop " + property.key + "=" + property.default_value;
    }
    for (const auto& i : cell.instances) {
      out += "\n inst " + i.name + " of " + db.cells.at(i.cell).name + ":";
      for (const auto net : i.connections) {
        out += " " + std::to_string(net);
      }
      for (const auto& record : i.properties) {
        for (const auto& p : record) {
          out += " " + p.key + "=" + p.value;
        }
      }
    }
    out += "\n";
  }
  return out;
}

}  // namespace lvs_test
