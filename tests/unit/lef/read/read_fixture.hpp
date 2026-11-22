// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared fixtures of the lef/read unit tests: a small technology, a
// hand-built LEF layer table behind lef::LayerAccess (the adapter over
// lef::LayerTable follows LEFTECH's table), and a LEF stream runner.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/lef/lex/lexer.hpp"
#include "hefesto/lef/read/layers.hpp"
#include "hefesto/lef/read/stream.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/tech/technology.hpp"

namespace lefread_test {

using namespace hefesto;

// Types: poly (active), metal1/m1 and metal2/m2, the contact m2c (via1)
// between them, and obsv1, an obstruction-only metal2 type.
inline constexpr std::string_view kTech = R"(tech
  format 35
  lefread
end
planes
  active,a
  metal1,m1
  metal2,m2
end
types
  active polysilicon,poly
  metal1 metal1,m1
  metal2 metal2,m2
  metal1 m2contact,m2c,via1
  metal2 obsv1
end
contact
  m2c metal1 metal2
end
styles
  styletype mos
  poly 1
  metal1 20
  metal2 21
end
compose
end
connect
end
cifoutput
end
cifinput
end
drc
end
extract
end
)";

inline std::shared_ptr<const tech::Technology> technology() {
  static const std::shared_ptr<const tech::Technology> loaded = [] {
    platform::MemoryFileSystem fs;
    diag::CollectingSink sink;
    auto result = tech::load_technology_text(kTech, "/t/lefread.tech", fs, tech::LoadOptions{}, sink);
    for (const auto& d : sink.take()) {
      UNSCOPED_INFO(d.code << ": " << d.text);
    }
    REQUIRE(result.ok());
    return result.value();
  }();
  return loaded;
}

inline tech::TileType type_of(std::string_view name) {
  const auto found = technology()->find_type(name);
  REQUIRE(found.ok());
  return *found.id;
}

/// A hand-built LEF layer table: aliases share one record.
class FakeLayers final : public lef::LayerAccess {
 public:
  struct Record {
    lef::LayerEntry entry;
    lef::RouteUpdate route;  // the route values, as last updated
    bool added = false;      // created by add_route
  };

  /// Adds `record` under every name of `names`.
  void define(const std::vector<std::string>& names, const lef::LayerEntry& entry) {
    auto record = std::make_shared<Record>(Record{entry, {}, false});
    for (const auto& name : names) {
      records[name] = record;
    }
  }

  [[nodiscard]] std::optional<lef::LayerEntry> find(std::string_view name) const override {
    const auto it = records.find(std::string{name});
    if (it == records.end()) {
      return std::nullopt;
    }
    return it->second->entry;
  }

  void add_route(std::string_view name, tech::TileType type) override {
    auto record = std::make_shared<Record>();
    record->entry.layer_class = lef::LayerClass::route;
    record->entry.type = type;
    record->added = true;
    records[std::string{name}] = record;
  }

  void update_route(std::string_view name, const lef::RouteUpdate& update) override {
    Record& record = *records.at(std::string{name});
    if (update.width) {
      record.route.width = update.width;
    }
    if (update.spacing) {
      record.route.spacing = update.spacing;
    }
    if (update.pitch) {
      record.route.pitch = update.pitch;
    }
    if (update.direction) {
      record.route.direction = update.direction;
    }
    ++updates;
  }

  void reserve(std::string_view name) override { reserved.emplace_back(name); }

  [[nodiscard]] bool keyed_without_entry(std::string_view name) const override {
    return !records.contains(std::string{name}) &&
           std::find(reserved.begin(), reserved.end(), std::string{name}) != reserved.end();
  }

  [[nodiscard]] const Record& at(const std::string& name) const { return *records.at(name); }

  std::map<std::string, std::shared_ptr<Record>> records;
  std::vector<std::string> reserved;
  int updates = 0;
};

/// The tech `lef` lines of lef-read's scenarios: `routing m1 metal1`,
/// `routing m2 metal2`, `cut m2c via1` with `obstruction obsv1,m1 via1`
/// (primary m1, secondary obsv1), and an IGNORE entry `pwell`.
inline void define_scenario_layers(FakeLayers& layers) {
  layers.define({"metal1", "m1"}, lef::LayerEntry{lef::LayerClass::route, type_of("metal1"), 0, 0, std::nullopt});
  layers.define({"metal2"}, lef::LayerEntry{lef::LayerClass::route, type_of("metal2"), 0, 0, std::nullopt});
  layers.define({"via1"}, lef::LayerEntry{lef::LayerClass::cut, type_of("m2c"), type_of("metal1"), type_of("obsv1"),
                                          db::Rect{-6, -6, 6, 6}});
  layers.define({"pwell"}, lef::LayerEntry{lef::LayerClass::ignore, 0, 0, 0, std::nullopt});
}

inline std::vector<std::byte> bytes(std::string_view text) {
  std::vector<std::byte> out;
  for (const char c : text) {
    out.push_back(static_cast<std::byte>(c));
  }
  return out;
}

/// One LEF read of `text` into its own layer, via and rule tables.
struct LefRun {
  explicit LefRun(bool magic_compat = true, float scale = 0.01F) {
    options.compat.magic_compat = magic_compat;
    options.scale = scale;
  }

  lef::StreamResult read(std::string_view text) {
    gds::MemorySource source{bytes(text)};
    lef::Lexer lexer{source};
    const lef::MessageTarget messages{counter, sink, lexer};
    lef::StreamTarget target{layers, vias, rules, nullptr, nullptr};
    return lef::read_stream(lexer, messages, options, target);
  }

  /// The texts of the messages so far, and clears them.
  std::vector<std::string> texts() {
    std::vector<std::string> out;
    for (const auto& d : sink.take()) {
      out.push_back(d.text);
    }
    return out;
  }

  [[nodiscard]] const lef::NonDefaultRule& rule(const std::string& name) const {
    const auto* entry = rules.find(name);
    REQUIRE(entry != nullptr);
    REQUIRE(entry->value.has_value());
    return *entry->value;
  }

  FakeLayers layers;
  lef::ViaTable vias;
  lef::RuleTable rules;
  lef::MessageCounter counter;
  diag::CollectingSink sink;
  lef::StreamOptions options{*technology(), 0.01F, {}, nullptr, {}};
};

using Strings = std::vector<std::string>;

}  // namespace lefread_test
