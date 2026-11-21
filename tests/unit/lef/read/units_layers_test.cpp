// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Unit tests of LEF unit conversion and LEF layer name resolution
// (capability lef-read; openspec/changes/m6-lef-def-oasis tasks.md 5.2).

#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "hefesto/lef/read/units.hpp"
#include "hefesto/mask/style/style.hpp"
#include "read_fixture.hpp"

using namespace lefread_test;

namespace {

float scale_of(std::int32_t scale_factor, std::int32_t expander) {
  mask::OutputStyle style;
  style.scale.scale_factor = scale_factor;
  style.scale.expander = expander;
  return lef::lef_scale(&style);
}

/// resolve_layer with its own message target.
struct Resolver {
  explicit Resolver(bool magic_compat = true) { compat.magic_compat = magic_compat; }

  void layer(std::string_view name, lef::LayerLookup lookup, lef::LayerSource from = lef::LayerSource::geometry) {
    const lef::MessageTarget messages{counter, sink, lexer};
    lef::resolve_layer(name, lookup, state, lef::LayerContext{layers, *technology(), compat, messages}, from);
  }

  std::vector<std::string> texts() {
    std::vector<std::string> out;
    for (const auto& d : sink.take()) {
      out.push_back(d.text);
    }
    return out;
  }

  FakeLayers layers;
  compat::CompatSettings compat;
  gds::MemorySource source{{}};
  lef::Lexer lexer{source};
  lef::MessageCounter counter;
  diag::CollectingSink sink;
  lef::LayerState state;
};

}  // namespace

TEST_CASE("the LEF scale is CIFGetOutputScale(1000)", "[cap:lef-read][req:LEF unit conversion]") {
  CHECK(lef::lef_scale(nullptr) == 1.0F);
  CHECK(scale_of(1, 1) == 0.01F);
  CHECK(scale_of(1, 2) == 0.005F);  // one internal unit is 5 nm
}

TEST_CASE("nanometer grid", "[cap:lef-read][req:LEF unit conversion]") {
  CHECK(lef::to_internal(0.0125F, scale_of(1, 2)) == 3);
}

TEST_CASE("no output style", "[cap:lef-read][req:LEF unit conversion]") {
  CHECK(lef::to_internal(2.4F, lef::lef_scale(nullptr)) == 2);
  CHECK(lef::to_internal(5.0F, lef::lef_scale(nullptr)) == 5);
}

TEST_CASE("LEFDEF-007: half away from zero in binary32 at .0025 boundaries",
          "[cap:lef-read][req:LEF unit conversion]") {
  const float s = scale_of(1, 2);
  CHECK(lef::to_internal(0.0025F, s) == 1);
  CHECK(lef::to_internal(-0.0025F, s) == -1);
  CHECK(lef::to_internal(0.0125F, s) == 3);
  CHECK(lef::to_internal(-0.0125F, s) == -3);
  CHECK(lef::to_internal(0.0075F, s) == 2);
  // binary32 quotients that double arithmetic rounds the other way.
  CHECK(lef::to_internal(0.0725F, s) == 15);
  CHECK(lef::to_internal(-0.1425F, s) == -29);
  CHECK(lef::to_internal(-0.2975F, s) == -60);
}

TEST_CASE("via geometry is in half units", "[cap:lef-read][req:LEF unit conversion]") {
  const float s = scale_of(1, 1);
  CHECK(lef::to_half_internal(0.005F, s) == 1);
  CHECK(lef::to_half_internal(0.07F, s) == 14);
  CHECK(lef::to_half_internal(-0.0125F, s) == -3);
}

TEST_CASE("a value that does not fit is no value", "[cap:lef-read][req:LEF unit conversion]") {
  CHECK_FALSE(lef::to_internal(3.0e7F, 0.01F));
  CHECK_FALSE(lef::to_internal(-3.0e7F, 0.01F));
  CHECK_FALSE(lef::to_internal(std::numeric_limits<float>::quiet_NaN(), 0.01F));
  CHECK(lef::to_internal(-2.0e7F, 1.0F) == -20000000);
}

TEST_CASE("fallback to magic type name", "[cap:lef-read][req:LEF layer name resolution]") {
  Resolver r;
  define_scenario_layers(r.layers);
  r.layer("METAL1", lef::LayerLookup::normal);
  CHECK(r.state.type == type_of("metal1"));
  CHECK(r.texts().empty());
}

// Regression: a key with no record (a name a NONDEFAULTRULE looked up, a
// released generated via) fell back to the magic type names in every
// lookup. Magic's LefReadLayer finds the key, gets no record and resolves
// no type (lefRead.c:715-756: no fallback, the unknown-layer errors); only
// LefReadLayers with result pointers (PORT and OBS) dereferences it and
// crashes, which Q-LEFDEF-4 fixes by treating the key as absent.
// Compat (Q-LEFDEF-151): magic-compat OFF treats it as absent everywhere.
TEST_CASE("a key with no record resolves to nothing in a LefReadLayer lookup with magic-compat ON",
          "[cap:lef-read][req:LEF layer name resolution]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << on);
    Resolver r(on);
    define_scenario_layers(r.layers);
    r.layers.reserve("poly");
    r.layer("poly", lef::LayerLookup::normal, lef::LayerSource::read_layer);
    CHECK((r.state.type == std::nullopt) == on);
    CHECK((r.texts() == Strings{"Don't know how to parse layer \"poly\"",
                                "Try adding this name to the LEF techfile section"}) == on);
    // PORT and OBS geometry: treated as absent in both modes (Q-LEFDEF-4).
    r.layer("poly", lef::LayerLookup::obstruction);
    CHECK(r.state.type == type_of("poly"));
    CHECK(r.texts().empty());
  }
}

TEST_CASE("table entries give the normal type and a cut's contact area", "[cap:lef-read][req:LEF layer name resolution]") {
  Resolver r;
  define_scenario_layers(r.layers);
  r.layer("m1", lef::LayerLookup::normal);  // an alias
  CHECK(r.state.type == type_of("metal1"));
  CHECK_FALSE(r.state.contact_area);
  r.layer("via1", lef::LayerLookup::normal);
  CHECK(r.state.type == type_of("m2c"));
  CHECK(r.state.contact_area == db::Rect{-6, -6, 6, 6});
  CHECK_FALSE(r.state.secondary);  // only an obstruction lookup sets it
}

TEST_CASE("obstruction lookup with second type", "[cap:lef-read][req:LEF layer name resolution]") {
  Resolver r;
  define_scenario_layers(r.layers);
  r.layer("via1", lef::LayerLookup::obstruction);
  CHECK(r.state.type == type_of("metal1"));
  CHECK(r.state.secondary == type_of("obsv1"));
  r.layer("metal1", lef::LayerLookup::obstruction);  // no obstruction type: the normal one
  CHECK(r.state.type == type_of("metal1"));
}

TEST_CASE("secondary obstruction type carries over in OBS", "[cap:lef-read][req:LEF layer name resolution]") {
  SECTION("magic-compat ON") {
    Resolver r{true};
    define_scenario_layers(r.layers);
    r.layer("via1", lef::LayerLookup::obstruction);
    r.layer("metal2", lef::LayerLookup::obstruction);
    CHECK(r.state.type == type_of("metal2"));
    CHECK(r.state.secondary == type_of("obsv1"));
  }
  SECTION("magic-compat OFF") {
    Resolver r{false};
    define_scenario_layers(r.layers);
    r.layer("via1", lef::LayerLookup::obstruction);
    r.layer("metal2", lef::LayerLookup::obstruction);
    CHECK(r.state.type == type_of("metal2"));
    CHECK_FALSE(r.state.secondary);
  }
}

TEST_CASE("the contact area carries over a magic-name LAYER", "[cap:lef-read][req:LEF layer name resolution]") {
  SECTION("magic-compat ON") {
    Resolver r{true};
    define_scenario_layers(r.layers);
    r.layer("via1", lef::LayerLookup::normal);
    r.layer("m2contact", lef::LayerLookup::normal);  // resolved through the magic names
    CHECK(r.state.type == type_of("m2c"));
    CHECK(r.state.contact_area == db::Rect{-6, -6, 6, 6});
    r.layer("metal2", lef::LayerLookup::normal);  // a table entry resets it
    CHECK_FALSE(r.state.contact_area);
  }
  SECTION("magic-compat OFF") {
    Resolver r{false};
    define_scenario_layers(r.layers);
    r.layer("via1", lef::LayerLookup::normal);
    r.layer("m2contact", lef::LayerLookup::normal);
    CHECK(r.state.type == type_of("m2c"));
    CHECK_FALSE(r.state.contact_area);
  }
}

TEST_CASE("unknown layer", "[cap:lef-read][req:LEF layer name resolution]") {
  Resolver r;
  define_scenario_layers(r.layers);
  r.layer("poly2", lef::LayerLookup::normal);
  CHECK_FALSE(r.state.type);
  CHECK(r.texts() == Strings{"LEF read, Line 0 (Error): Don't know how to parse layer \"poly2\"",
                             "LEF read, Line 0 (Error): Try adding this name to the LEF techfile section"});
}

TEST_CASE("an IGNORE entry resolves to no type silently", "[cap:lef-read][req:LEF layer name resolution]") {
  Resolver r;
  define_scenario_layers(r.layers);
  r.layer("pwell", lef::LayerLookup::normal);
  CHECK_FALSE(r.state.type);
  r.layer("pwell", lef::LayerLookup::obstruction);
  CHECK_FALSE(r.state.type);
  CHECK(r.texts().empty());
}

TEST_CASE("bad Layer statement", "[cap:lef-read][req:LEF layer name resolution]") {
  Resolver r;
  define_scenario_layers(r.layers);
  r.layer("via1", lef::LayerLookup::obstruction);
  r.layer(";", lef::LayerLookup::obstruction);
  CHECK_FALSE(r.state.type);
  CHECK(r.state.secondary == type_of("obsv1"));  // ON: untouched
  CHECK(r.texts() == Strings{"LEF read, Line 0 (Error): Bad Layer statement"});
}

TEST_CASE("an obstruction-only entry has no type for pins", "[cap:lef-read][req:LEF layer name resolution]") {
  Resolver r;
  r.layers.define({"obs"}, lef::LayerEntry{lef::LayerClass::route, 0, type_of("obsv1"), 0, std::nullopt});
  r.layer("obs", lef::LayerLookup::obstruction);
  CHECK(r.state.type == type_of("obsv1"));
  r.layer("obs", lef::LayerLookup::normal);
  CHECK_FALSE(r.state.type);
  CHECK(r.texts().size() == 2);
}
