// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DRC on generated mask layers (capability drc-cif-rules; M5 design D14):
// the compiled primitive rules, the mask-layer check run at the end of
// basic_check, the error mapping of Q-DRC-13 in both modes, the area and
// maxwidth rules, and malformed rule values.
//
// The technology's output style `mask` has scalefactor 10 (σ = 10), so
// layout [a,b] is mask [10a,10b]; `shrink 3` gives sub-unit mask shapes.
//
// Owner: DRCCIF (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/lock.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/drc/check/basic_check.hpp"
#include "hefesto/drc/check/mask_check.hpp"
#include "hefesto/drc/engine/background.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/drc/rules/cif_catalog.hpp"
#include "hefesto/drc/rules/details.hpp"
#include "hefesto/drc/rules/style.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/mask/gen/context.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::drc {
namespace {

using db::Rect;

constexpr std::string_view kTechHead = R"(tech
    format 35
    masktest
end

planes
    active
    metal1
end

types
    active  ndiff
    metal1  m1
    metal1  via
end

contact
end

styles
    styletype mos
end

compose
end

connect
end

cifoutput
style mask
    scalefactor 10
    layer NW ndiff
    layer MET1 m1
    layer VIA1 via
        shrink 3
    layer VIA2 via
    layer PW ndiff
style other
    scalefactor 10
    layer MET1 m1
end

cifinput
end
)";

constexpr std::string_view kDrcSection = R"(drc
style width
    cifstyle mask
    cifwidth MET1 140 "m1 width"
style spacing
    cifstyle mask
    cifspacing MET1 MET1 100 touching_ok "m1 spacing"
style wells
    cifstyle mask
    cifspacing NW PW 600 touching_illegal "well spacing"
style area
    cifstyle mask
    cifarea VIA1 10000 50 "via area"
style area2
    cifstyle mask
    cifarea VIA2 10000 50 "via area"
style bendok
    cifstyle mask
    cifmaxwidth MET1 3000 bend_ok "slot"
style bendill
    cifstyle mask
    cifmaxwidth MET1 3000 bend_illegal "slot"
style both
    cifstyle mask
    cifmaxwidth MET1 3000 both "slot"
style unbound
    cifstyle other
    cifwidth MET1 140 "m1 width"
style missing
    cifwidth MET1 140 "m1 width"
style huge
    cifstyle mask
    cifwidth MET1 2147483647 "huge"
    cifspacing MET1 MET1 2147483647 touching_illegal "huge spacing"
    cifarea VIA2 2147483647 2147483647 "huge area"
style negative
    cifstyle mask
    cifwidth MET1 -2147483647 "negative"
end

extract
end
)";

std::shared_ptr<const tech::Technology> mask_tech() {
  static const std::shared_ptr<const tech::Technology> instance = [] {
    platform::MemoryFileSystem fs;
    diag::CollectingSink sink;
    const std::string text = std::string{kTechHead} + std::string{kDrcSection};
    auto loaded = tech::load_technology_text(text, "/t/masktest.tech", fs, tech::LoadOptions{}, sink);
    if (!loaded.ok()) {
      for (const auto& d : sink.take()) {
        UNSCOPED_INFO(d.code << ": " << d.text);
      }
    }
    REQUIRE(loaded.ok());
    return loaded.value();
  }();
  return instance;
}

compat::CompatSettings mode(bool magic_compat) {
  compat::CompatSettings c;
  c.magic_compat = magic_compat;
  return c;
}

/// One compiled DRC style of the mask technology and a flat cell to paint.
struct MaskCase {
  std::shared_ptr<const tech::Technology> tech = mask_tech();
  CifCatalog cif;
  std::shared_ptr<const RuleSet> rules;
  std::vector<diag::Diagnostic> messages;
  db::CellDef cell{"top", mask_tech()->plane_count()};

  explicit MaskCase(std::string_view style, bool magic_compat = true) {
    diag::CollectingSink sink;
    cif = CifCatalog::from_technology(*tech, sink);
    rules = load_style(*tech, style, tech::GridScale{}, cif, mode(magic_compat), sink);
    messages = sink.take();
    REQUIRE(rules != nullptr);
  }

  void paint(std::string_view type, const Rect& r) {
    const auto found = tech->find_type(type);
    REQUIRE(found.ok());
    REQUIRE(db::paint_type(cell, *tech, r, *found.id).ok());
  }

  [[nodiscard]] CheckInput input(bool magic_compat) const {
    CheckInput in;
    in.cell = &cell;
    in.technology = tech.get();
    in.rules = rules.get();
    in.cif = &cif;
    in.compat = mode(magic_compat);
    return in;
  }

  /// basic_check over K with clip L; the error rectangles in report order.
  [[nodiscard]] std::vector<Rect> check(const Rect& k, const Rect& l, bool magic_compat = true,
                                        bool euclidean = false) const {
    CollectingErrorSink sink;
    CheckInput in = input(magic_compat);
    in.euclidean = euclidean;
    REQUIRE(basic_check(in, k, l, sink) == CheckOutcome::completed);
    std::vector<Rect> out;
    for (const auto& e : sink.errors()) {
      out.push_back(e.area);
    }
    return out;
  }

  [[nodiscard]] std::string why_of(const Rule& r) const { return std::string{rules->why().text(r.why)}; }
};

constexpr Rect kWide{-1000, -1000, 1000, 1000};

bool has_rule(const std::vector<MaskRule>& list, RuleFlags rule_flags, std::int32_t dist, std::size_t check_layer) {
  for (const MaskRule& m : list) {
    if (m.rule.flags == (rule_flags | flags::kCifRule) && m.rule.dist == dist && m.check_layer == check_layer) {
      return true;
    }
  }
  return false;
}

// --- Mask style binding ----------------------------------------------------------

TEST_CASE("mask-layer rules without a cifstyle are ignored with one message",
          "[cap:drc-cif-rules][req:Mask style binding]") {
  MaskCase c("missing");
  std::size_t said = 0;
  for (const auto& d : c.messages) {
    said += d.text.find("Missing cif style for drc") != std::string::npos ? 1 : 0;
  }
  CHECK(said == 1);
  REQUIRE(c.rules->details() != nullptr);
  CHECK(c.rules->details()->mask_layer_rules.empty());
  c.paint("m1", Rect{0, 0, 10, 100});
  CHECK(c.check(kWide, kWide).empty());
}

TEST_CASE("a cifstyle that is not current compiles nothing until the style is current",
          "[cap:drc-cif-rules][req:Mask style binding]") {
  MaskCase c("unbound");
  REQUIRE(c.rules->details() != nullptr);
  CHECK_FALSE(c.rules->details()->cif_bound);
  c.paint("m1", Rect{0, 0, 10, 100});
  CHECK(c.check(kWide, kWide).empty());

  // Selecting the bound style recompiles the rules (DRCReloadCurStyle).
  REQUIRE(c.cif.select("other").ok());
  diag::CollectingSink sink;
  c.rules = load_style(*c.tech, "unbound", tech::GridScale{}, c.cif, mode(true), sink);
  REQUIRE(c.rules->details()->cif_bound);
  CHECK(c.check(kWide, kWide) == std::vector<Rect>{{10, 0, 14, 100}});

  // drcCifCheck generates with the bound style even when another output
  // style is current again (CIFSaveStyle).
  REQUIRE(c.cif.select("mask").ok());
  CHECK(c.check(kWide, kWide) == std::vector<Rect>{{10, 0, 14, 100}});
}

// --- Mask-layer rule statements -------------------------------------------------

TEST_CASE("cifspacing touching_illegal compiles Magic's cookies", "[cap:drc-cif-rules][req:Mask-layer rule statements]") {
  MaskCase c("wells");
  const auto style = c.cif.output_style("mask");
  REQUIRE(style != nullptr);
  const MaskRules rules = compile_mask_rules(*c.rules->details(), *style);
  const std::size_t nw = *style->find_layer("NW");
  const std::size_t pw = *style->find_layer("PW");
  const auto& nw_space = rules.list(nw, MaskList::space_tiles);
  const auto& nw_solid = rules.list(nw, MaskList::solid_tiles);
  CHECK(has_rule(nw_space, flags::kForward | flags::kBothCorners, 600, pw));
  CHECK(has_rule(nw_solid, flags::kReverse | flags::kBothCorners, 600, pw));
  CHECK(has_rule(nw_solid, flags::kForward, 10, pw));  // coincident material, dist σ
  CHECK(has_rule(rules.list(pw, MaskList::space_tiles), flags::kForward | flags::kBothCorners, 600, nw));
  CHECK(has_rule(rules.list(pw, MaskList::solid_tiles), flags::kReverse | flags::kBothCorners, 600, nw));
  CHECK(has_rule(rules.list(pw, MaskList::solid_tiles), flags::kForward, 10, nw));
  CHECK(nw_space.size() == 1);
  CHECK(nw_solid.size() == 2);
  // Pushed at the front: the σ rule (compiled last) comes first.
  CHECK(nw_solid.front().rule.dist == 10);
  for (const MaskRule& m : nw_space) {
    CHECK(m.rule.ok == tech::TypeMask::of(tech::kSpace));
    CHECK(m.rule.corner == tech::TypeMask::all());
    CHECK(c.why_of(m.rule) == "well spacing");
  }
  CHECK(rules.used[nw]);
  CHECK(rules.used[pw]);
  CHECK_FALSE(rules.used[*style->find_layer("MET1")]);
  // Halo contribution: ceil(600 / 10).
  CHECK(c.rules->halo().mask_contribution == 60);
}

TEST_CASE("cifwidth, cifarea and cifmaxwidth compile onto the solid-tile list",
          "[cap:drc-cif-rules][req:Mask-layer rule statements]") {
  const auto style = MaskCase("width").cif.output_style("mask");
  REQUIRE(style != nullptr);
  const std::size_t met1 = *style->find_layer("MET1");
  const std::size_t via1 = *style->find_layer("VIA1");

  const MaskRules width = compile_mask_rules(*MaskCase("width").rules->details(), *style);
  REQUIRE(width.list(met1, MaskList::solid_tiles).size() == 1);
  const Rule& w = width.list(met1, MaskList::solid_tiles)[0].rule;
  CHECK(w.flags == (flags::kForward | flags::kCifRule));
  CHECK(w.dist == 140);
  CHECK(w.cdist == 140);
  CHECK(w.ok == tech::TypeMask::of(mask::kMaskSolid));
  CHECK(w.corner == tech::TypeMask::of(mask::kMaskSolid));
  CHECK(width.list(met1, MaskList::space_tiles).empty());

  const MaskRules area = compile_mask_rules(*MaskCase("area").rules->details(), *style);
  REQUIRE(area.list(via1, MaskList::solid_tiles).size() == 1);
  const Rule& a = area.list(via1, MaskList::solid_tiles)[0].rule;
  CHECK(a.flags == (flags::kArea | flags::kCifRule));
  CHECK(a.dist == 50);      // horizon
  CHECK(a.cdist == 10000);  // area

  const MaskRules bends = compile_mask_rules(*MaskCase("bendok").rules->details(), *style);
  CHECK(bends.list(met1, MaskList::solid_tiles)[0].rule.flags == (flags::kMaxWidth | flags::kBends | flags::kCifRule));
  const MaskRules both = compile_mask_rules(*MaskCase("both").rules->details(), *style);
  CHECK(both.list(met1, MaskList::solid_tiles)[0].rule.flags ==
        (flags::kMaxWidth | flags::kMaxWidthBoth | flags::kCifRule));
}

TEST_CASE("a style without the rule's layer compiles nothing for it",
          "[cap:drc-cif-rules][req:Mask-layer rule statements]") {
  MaskCase c("area");
  const auto other = c.cif.output_style("other");  // has MET1 only
  REQUIRE(other != nullptr);
  CHECK(compile_mask_rules(*c.rules->details(), *other).empty());
}

// --- Mask-layer check procedure -------------------------------------------------

TEST_CASE("a narrow generated layer violates cifwidth", "[cap:drc-cif-rules][req:Mask-layer check procedure]") {
  MaskCase c("width");
  c.paint("m1", Rect{0, 0, 10, 100});  // MET1 [0,100]x[0,1000] mask units
  CollectingErrorSink sink;
  REQUIRE(basic_check(c.input(true), kWide, kWide, sink) == CheckOutcome::completed);
  REQUIRE(sink.errors().size() == 1);
  CHECK(sink.errors()[0].area == Rect{10, 0, 14, 100});  // from the candidate [100,140]x[0,1000]
  CHECK(c.why_of(sink.errors()[0].rule) == "m1 width");
  CHECK(sink.errors()[0].rule.has(flags::kCifRule));

  // A wide enough rectangle is clean.
  MaskCase wide("width");
  wide.paint("m1", Rect{0, 0, 14, 100});
  CHECK(wide.check(kWide, kWide).empty());
}

TEST_CASE("mask spacing is checked between generated shapes", "[cap:drc-cif-rules][req:Mask-layer check procedure]") {
  MaskCase c("spacing");
  c.paint("m1", Rect{0, 0, 20, 20});
  c.paint("m1", Rect{25, 0, 45, 20});  // 50 mask units apart, rule 100
  // Each side reports the other's material within the spacing: [15,20]
  // (the reverse rule from x = 25) and [25,30] (the forward rule from x = 20).
  const auto errors = c.check(kWide, kWide);
  REQUIRE_FALSE(errors.empty());
  for (const Rect& e : errors) {
    CHECK(((e.x0 == 15 && e.x1 == 20) || (e.x0 == 25 && e.x1 == 30)));
  }
  MaskCase apart("spacing");
  apart.paint("m1", Rect{0, 0, 20, 20});
  apart.paint("m1", Rect{30, 0, 50, 20});  // exactly 100 apart
  CHECK(apart.check(kWide, kWide).empty());
}

TEST_CASE("the Euclidean corner filter exempts diagonal neighbours beyond the radius",
          "[cap:drc-cif-rules][req:Mask-layer check procedure]") {
  MaskCase c("spacing");
  c.paint("m1", Rect{0, 0, 10, 10});
  c.paint("m1", Rect{18, 18, 28, 28});  // 80 x 80 mask units apart diagonally: 113 > 100
  CHECK_FALSE(c.check(kWide, kWide, true, false).empty());
  CHECK(c.check(kWide, kWide, true, true).empty());

  MaskCase close("spacing");
  close.paint("m1", Rect{0, 0, 10, 10});
  close.paint("m1", Rect{16, 16, 26, 26});  // 60 x 60: 85 < 100
  CHECK_FALSE(close.check(kWide, kWide, true, true).empty());
}

TEST_CASE("generation is clipped to K and errors are clipped to L",
          "[cap:drc-cif-rules][req:Mask-layer check procedure]") {
  MaskCase c("width");
  c.paint("m1", Rect{0, 0, 10, 100});
  // CIFGen clips every layer to K * σ, so K's left side becomes an edge of
  // the clipped MET1 [50,100] (as in Magic).
  CHECK(c.check(Rect{5, -10, 50, 110}, kWide) == std::vector<Rect>{{10, 0, 19, 100}});
  // A K that misses the shape checks nothing.
  CHECK(c.check(Rect{20, -10, 50, 110}, kWide).empty());
  // L cuts the error.
  CHECK(c.check(kWide, Rect{-100, 50, 100, 60}) == std::vector<Rect>{{10, 50, 14, 60}});
}

TEST_CASE("the mask-layer check counts its tiles and stops on cancel",
          "[cap:drc-cif-rules][req:Mask-layer check procedure]") {
  MaskCase c("width");
  c.paint("m1", Rect{0, 0, 10, 100});
  CollectingErrorSink sink;
  CheckStatistics stats;
  sink.set_statistics(&stats);
  REQUIRE(mask_layer_check(c.input(true), kWide, kWide, sink) == CheckOutcome::completed);
  CHECK(stats.cif_tiles > 0);
  CHECK(stats.tiles == stats.cif_tiles);

  std::atomic<bool> stop{true};
  CheckInput in = c.input(true);
  in.cancel.flag = &stop;
  CollectingErrorSink cancelled;
  CHECK(mask_layer_check(in, kWide, kWide, cancelled) == CheckOutcome::cancelled);
}

TEST_CASE("the engine's generation context is reused across checks",
          "[cap:drc-cif-rules][req:Mask-layer check procedure]") {
  MaskCase c("width");
  c.paint("m1", Rect{0, 0, 10, 100});
  mask::GenerationContext ctx;
  CheckInput in = c.input(true);
  in.generation = &ctx;
  CollectingErrorSink first;
  REQUIRE(mask_layer_check(in, kWide, kWide, first) == CheckOutcome::completed);
  const std::size_t planes = ctx.pool.size();
  CHECK(planes > 0);
  CollectingErrorSink second;
  REQUIRE(mask_layer_check(in, kWide, kWide, second) == CheckOutcome::completed);
  CHECK(ctx.pool.size() == planes);  // the planes went back to the pool
  CHECK(first.errors() == second.errors());
}

TEST_CASE("a style without mask-layer rules generates nothing", "[cap:drc-cif-rules][req:Mask-layer check procedure]") {
  MaskCase c("missing");
  c.paint("m1", Rect{0, 0, 10, 100});
  mask::GenerationContext ctx;
  CheckInput in = c.input(true);
  in.generation = &ctx;
  CollectingErrorSink sink;
  CHECK(mask_layer_check(in, kWide, kWide, sink) == CheckOutcome::completed);
  CHECK(ctx.pool.size() == 0);
  in.cif = nullptr;  // no catalog at all
  CHECK(mask_layer_check(in, kWide, kWide, sink) == CheckOutcome::completed);
  CHECK(sink.errors().empty());
}

// --- Mapping mask errors to layout coordinates ----------------------------------

TEST_CASE("sub-unit edge errors are widened with magic-compat ON",
          "[cap:drc-cif-rules][req:Mapping mask errors to layout coordinates]") {
  const auto on = mode(true);
  CHECK(map_mask_error(Rect{103, 0, 107, 50}, 10, MaskErrorKind::edge, on) == Rect{10, 0, 11, 5});
  CHECK(map_mask_error(Rect{-15, 0, -5, 10}, 10, MaskErrorKind::edge, on) == Rect{-1, 0, 0, 1});
  // Truncation toward zero puts [-7,-3] at 0, which widens upwards; a zero
  // extent below zero widens downwards.
  CHECK(map_mask_error(Rect{-7, -7, -3, -3}, 10, MaskErrorKind::edge, on) == Rect{0, 0, 1, 1});
  CHECK(map_mask_error(Rect{-17, 0, -13, 10}, 10, MaskErrorKind::edge, on) == Rect{-2, 0, -1, 1});
  // Region errors are truncated and dropped when they collapse.
  CHECK_FALSE(map_mask_error(Rect{103, 0, 107, 90}, 10, MaskErrorKind::region, on).has_value());
  CHECK(map_mask_error(Rect{0, 0, 90, 90}, 10, MaskErrorKind::region, on) == Rect{0, 0, 9, 9});
  CHECK(collapses_when_truncated(Rect{103, 0, 107, 90}, 10));
  CHECK_FALSE(collapses_when_truncated(Rect{0, 0, 90, 90}, 10));
}

TEST_CASE("errors are rounded outward with magic-compat OFF",
          "[cap:drc-cif-rules][req:Mapping mask errors to layout coordinates]") {
  const auto off = mode(false);
  CHECK(map_mask_error(Rect{-15, 0, -5, 10}, 10, MaskErrorKind::edge, off) == Rect{-2, 0, 0, 1});
  CHECK(map_mask_error(Rect{103, 0, 107, 50}, 10, MaskErrorKind::edge, off) == Rect{10, 0, 11, 5});
  CHECK(map_mask_error(Rect{103, 0, 107, 90}, 10, MaskErrorKind::region, off) == Rect{10, 0, 11, 9});
  CHECK(map_mask_error(Rect{-25, -25, 25, 25}, 10, MaskErrorKind::region, off) == Rect{-3, -3, 3, 3});
  // A scale below 1 is treated as 1 (never divides by zero).
  CHECK(map_mask_error(Rect{1, 2, 3, 4}, 0, MaskErrorKind::region, off) == Rect{1, 2, 3, 4});
}

TEST_CASE("a collapsed area error is dropped and counted with magic-compat ON",
          "[cap:drc-cif-rules][req:Mapping mask errors to layout coordinates]") {
  MaskCase c("area");
  c.paint("via", Rect{10, 0, 11, 9});  // VIA1 = shrink 3: [103,107]x[3,87], area 336
  diag::CollectingSink diags;
  mask::GenerationContext ctx;
  ctx.sink = &diags;
  CheckInput in = c.input(true);
  in.generation = &ctx;
  CollectingErrorSink sink;
  REQUIRE(basic_check(in, kWide, kWide, sink) == CheckOutcome::completed);
  CHECK(sink.errors().empty());
  const auto messages = diags.take();
  REQUIRE(messages.size() == 1);
  CHECK(messages[0].code == "DRC-MASK-001");
  CHECK(messages[0].native_only);
  CHECK(messages[0].text.rfind("1 mask-layer", 0) == 0);
}

TEST_CASE("a collapsed area error is reported rounded outward and counted with magic-compat OFF",
          "[cap:drc-cif-rules][req:Mapping mask errors to layout coordinates]") {
  MaskCase c("area", false);
  c.paint("via", Rect{10, 0, 11, 9});
  diag::CollectingSink diags;
  mask::GenerationContext ctx;
  ctx.sink = &diags;
  CheckInput in = c.input(false);
  in.generation = &ctx;
  CollectingErrorSink sink;
  REQUIRE(basic_check(in, kWide, kWide, sink) == CheckOutcome::completed);
  REQUIRE(sink.errors().size() == 1);
  CHECK(sink.errors()[0].area == Rect{10, 0, 11, 9});
  const auto messages = diags.take();
  REQUIRE(messages.size() == 1);
  CHECK(messages[0].code == "DRC-MASK-002");
  CHECK(messages[0].native_only);
}

// --- Mask area rule ----------------------------------------------------------------

TEST_CASE("a small isolated via violates cifarea", "[cap:drc-cif-rules][req:Mask area rule]") {
  MaskCase c("area2");
  c.paint("via", Rect{0, 0, 9, 9});  // VIA2 [0,90]x[0,90]: 8100 < 10000
  CHECK(c.check(kWide, kWide) == std::vector<Rect>{{0, 0, 9, 9}});

  MaskCase big("area2");
  big.paint("via", Rect{0, 0, 10, 10});  // exactly 10000
  CHECK(big.check(kWide, kWide).empty());
}

TEST_CASE("every tile of an undersized region is reported", "[cap:drc-cif-rules][req:Mask area rule]") {
  MaskCase c("area2");
  c.paint("via", Rect{0, 0, 4, 4});
  c.paint("via", Rect{0, 4, 8, 6});  // an L of 3 tiles in the mask plane, 32 + 16 internal units^2
  const auto errors = c.check(kWide, kWide);
  CHECK(errors.size() >= 2);
  for (const Rect& e : errors) {
    CHECK(e.x0 >= 0);
    CHECK(e.x1 <= 8);
    CHECK(e.y1 <= 6);
  }
}

TEST_CASE("the area flood gives up at the edge of K", "[cap:drc-cif-rules][req:Mask area rule]") {
  MaskCase c("area2");
  c.paint("via", Rect{0, 0, 9, 9});
  // K whose bottom edge coincides with the via's bottom: no error.
  CHECK(c.check(Rect{-10, 0, 20, 20}, kWide).empty());
}

// --- Mask maxwidth rule ------------------------------------------------------------

TEST_CASE("bend_ok reports a tile wider than the limit in both directions", "[cap:drc-cif-rules][req:Mask maxwidth rule]") {
  MaskCase c("bendok");
  c.paint("m1", Rect{0, 0, 350, 350});
  CHECK(c.check(Rect{-1000, -1000, 5000, 5000}, Rect{-1000, -1000, 5000, 5000}) ==
        std::vector<Rect>{{0, 0, 350, 350}});
  MaskCase thin("bendok");
  thin.paint("m1", Rect{0, 0, 10, 500});
  CHECK(thin.check(Rect{-1000, -1000, 5000, 5000}, Rect{-1000, -1000, 5000, 5000}).empty());
}

TEST_CASE("bend_illegal and both follow Magic's region and tile rules", "[cap:drc-cif-rules][req:Mask maxwidth rule]") {
  const Rect area{-100, -100, 1000, 1000};
  // An L of two narrow bars whose bounding box exceeds 300 x 300.
  const auto paint_l = [](MaskCase& c) {
    c.paint("m1", Rect{400, 0, 410, 500});
    c.paint("m1", Rect{400, 400, 900, 410});
  };
  MaskCase illegal("bendill");
  paint_l(illegal);
  CHECK(illegal.check(area, area).size() >= 2);  // every tile of the region

  MaskCase bendok("bendok");
  paint_l(bendok);
  CHECK(bendok.check(area, area).empty());  // no single tile is wide both ways

  MaskCase both("both");
  paint_l(both);
  const auto errors = both.check(area, area);
  CHECK_FALSE(errors.empty());  // a tile longer than 300 in one direction
}

// --- Threads ---------------------------------------------------------------------

TEST_CASE("concurrent mask-layer checks with their own contexts agree",
          "[cap:drc-cif-rules][req:Mask-layer check procedure]") {
  // The background worker and the command thread each generate with their
  // own GenerationContext over the same layout, rules and immutable style
  // (design D14); the tsan leg runs this.
  MaskCase c("spacing");
  for (int i = 0; i < 6; ++i) {
    c.paint("m1", Rect{i * 25, 0, i * 25 + 20, 20});
    c.paint("m1", Rect{i * 25, 30, i * 25 + 18, 60});
  }
  const std::vector<Rect> expected = c.check(kWide, kWide);
  REQUIRE_FALSE(expected.empty());
  constexpr int kThreads = 4;
  std::vector<std::vector<Rect>> results(kThreads);
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&c, &results, t] {
      mask::GenerationContext ctx;
      CheckInput in = c.input(true);
      in.generation = &ctx;
      for (int round = 0; round < 10; ++round) {
        CollectingErrorSink sink;
        if (basic_check(in, kWide, kWide, sink) != CheckOutcome::completed) {
          return;
        }
        results[static_cast<std::size_t>(t)].clear();
        for (const auto& e : sink.errors()) {
          results[static_cast<std::size_t>(t)].push_back(e.area);
        }
      }
    });
  }
  for (std::thread& th : threads) {
    th.join();
  }
  for (const auto& r : results) {
    CHECK(r == expected);
  }
}

TEST_CASE("the background worker paints mask-layer errors", "[cap:drc-cif-rules][req:Mask-layer check procedure]") {
  compat::CompatSettings settings;
  editor::EditorState ed;
  ed.technology = mask_tech();
  ed.drc.compat = &settings;
  diag::CollectingSink sink;
  REQUIRE(reload_rules(ed, sink));  // the first DRC style is `width`
  auto made = ed.cells.create("top", ed.technology->plane_count());
  REQUIRE(made.ok());
  db::CellDef& top = *made.value();
  top.flags().available = true;
  const auto m1 = ed.technology->find_type("m1");
  REQUIRE(m1.ok());
  {
    const db::DatabaseLock::Exclusive guard(ed.database_lock);
    REQUIRE(db::paint_type(top, *ed.technology, Rect{0, 0, 10, 100}, *m1.id).ok());
    db::recompute_bbox(top);
    check_this(ed, top, Rect{0, 0, 10, 100});
  }
  BackgroundChecker& worker = *ed.drc.worker;
  worker.start(ed);
  worker.wake();
  REQUIRE(worker.run_until_idle(ed, 20000));
  worker.stop();
  std::vector<Rect> painted;
  {
    const db::DatabaseLock::Exclusive guard(ed.database_lock);
    top.plane(tech::kPlaneDrcError).enumerate(db::whole_plane(), tech::TypeMask::of(tech::kErrorP),
                                              [&](const db::TileView& t) {
                                                painted.push_back(t.area);
                                                return true;
                                              });
  }
  CHECK(painted == std::vector<Rect>{{10, 0, 14, 100}});
}

// --- Malformed rule values ----------------------------------------------------------

TEST_CASE("extreme mask-layer distances neither wrap nor crash", "[cap:drc-cif-rules][req:Mask-layer check procedure]") {
  for (const bool compat : {true, false}) {
    MaskCase c("huge", compat);
    c.paint("m1", Rect{0, 0, 10, 100});
    c.paint("m1", Rect{30, 0, 40, 100});
    c.paint("via", Rect{60, 0, 70, 10});
    const auto errors = c.check(kWide, kWide, compat, true);
    CHECK_FALSE(errors.empty());
    for (const Rect& e : errors) {
      CHECK(e.x0 < e.x1);
      CHECK(e.y0 < e.y1);
      CHECK(e.x0 >= kWide.x0);
      CHECK(e.x1 <= kWide.x1);
    }
    // K at the edge of the plane: K * σ is clamped.
    const Rect edge{db::kPlaneMax - 10, db::kPlaneMax - 10, db::kPlaneMax, db::kPlaneMax};
    CollectingErrorSink sink;
    CHECK(mask_layer_check(c.input(compat), edge, edge, sink) == CheckOutcome::completed);
  }
  MaskCase negative("negative");
  negative.paint("m1", Rect{0, 0, 10, 100});
  CHECK(negative.check(kWide, kWide).empty());
}

}  // namespace
}  // namespace hefesto::drc
