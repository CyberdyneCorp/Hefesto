// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// UniqueRenamer (capability extract-nodes-and-labels "Unique naming of
// duplicate labels"; openspec/changes/m7-extraction tasks.md 5.4):
// Q-EXTRACT-23 (replicate) and Q-EXTRACT-6 (fix, both modes). Owner: EXTNODE.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/extract/node/unique.hpp"
#include "node_fixture.hpp"

namespace hefesto::test::extnode {
namespace {

struct Duplicates {
  std::shared_ptr<const tech::Technology> t = case_tech("EX-019");
  extract::ExtStyle style;
  std::unique_ptr<db::CellDef> cell = new_cell(*t, "top");

  explicit Duplicates(bool magic_compat = true) : style(first_style(*t, magic_compat)) {
    paint(*cell, *t, "metal1", {0, 0, 10, 10});
    paint(*cell, *t, "metal1", {20, 0, 30, 10});
  }

  [[nodiscard]] extract::UniqueContext context(bool temporary, bool magic_compat = true) const {
    extract::UniqueContext ctx;
    ctx.technology = t.get();
    ctx.style = &style;
    ctx.temporary = temporary;
    ctx.compat = compat_mode(magic_compat);
    return ctx;
  }

  [[nodiscard]] std::vector<std::string> texts() const {
    std::vector<std::string> out;
    for (const db::Label& l : cell->labels()) {
      out.push_back(l.text);
    }
    return out;
  }
};

void temporary_unique_round(bool magic_compat) {
  Duplicates d(magic_compat);
  label(*d.cell, *d.t, "net1", "metal1", {5, 5, 5, 5});
  label(*d.cell, *d.t, "net1", "metal1", {25, 5, 25, 5});
  d.cell->flags().modified = false;
  d.cell->flags().needs_new_timestamp = false;
  extract::UniqueRenamer renamer;
  REQUIRE(renamer.apply(*d.cell, extract::UniqueMode::all, d.context(true, magic_compat)).ok());
  std::vector<std::string> renamed = d.texts();
  std::sort(renamed.begin(), renamed.end());
  CHECK(renamed == std::vector<std::string>{"net1", "net1_uq0"});
  CHECK(d.cell->flags().modified);

  // The `.ext` written while the labels are renamed has both names.
  Flat flat;
  extract_flat(flat, *d.cell, *d.t, d.style, magic_compat);
  CHECK(flat.text.find("node \"net1\"") != std::string::npos);
  CHECK(flat.text.find("node \"net1_uq0\"") != std::string::npos);
  CHECK(read_back("top", flat.text).empty());

  // Q-EXTRACT-6: the top cell is reverted, and left unmodified.
  renamer.revert();
  CHECK(d.texts() == std::vector<std::string>{"net1", "net1"});
  CHECK_FALSE(d.cell->flags().modified);
  CHECK_FALSE(d.cell->flags().needs_new_timestamp);
}

TEST_CASE("Q-EXTRACT-6: temporary unique labels of the top cell are reverted, magic-compat ON",
          "[cap:extract-nodes-and-labels][req:Unique naming of duplicate labels]") {
  temporary_unique_round(true);
}

TEST_CASE("Q-EXTRACT-6: temporary unique labels of the top cell are reverted, magic-compat OFF",
          "[cap:extract-nodes-and-labels][req:Unique naming of duplicate labels]") {
  temporary_unique_round(false);
}

TEST_CASE("Q-EXTRACT-23: the node visited later keeps the name",
          "[cap:extract-nodes-and-labels][req:Unique naming of duplicate labels]") {
  Duplicates d;
  label(*d.cell, *d.t, "x#", "metal1", {5, 5, 5, 5});
  label(*d.cell, *d.t, "x#", "metal1", {25, 5, 25, 5});
  extract::UniqueRenamer renamer;
  REQUIRE(renamer.apply(*d.cell, extract::UniqueMode::tagged, d.context(false)).ok());
  const extract::NodeRegionList nodes = nodes_of(*d.cell, *d.t, d.style);
  REQUIRE(nodes.size() == 2);
  // Node list order: the later node in the list kept `x#`.
  CHECK(nodes[1].labels.at(0).text == "x#");
  CHECK(nodes[0].labels.at(0).text == "x#_uq0");
  // A permanent rename is not reverted.
  renamer.revert();
  CHECK(nodes_of(*d.cell, *d.t, d.style)[0].labels.at(0).text == "x#_uq0");
}

TEST_CASE("Q-EXTRACT-23: a port on the earlier node keeps the name over a non-port",
          "[cap:extract-nodes-and-labels][req:Unique naming of duplicate labels]") {
  Duplicates d;
  // The first node of the list holds the port.
  const extract::NodeRegionList before = nodes_of(*d.cell, *d.t, d.style);
  const bool right_first = before[0].reference.x == 20;
  label(*d.cell, *d.t, "out", "metal1", right_first ? db::Rect{25, 5, 25, 5} : db::Rect{5, 5, 5, 5}, 4);
  label(*d.cell, *d.t, "out", "metal1", right_first ? db::Rect{5, 5, 5, 5} : db::Rect{25, 5, 25, 5});
  extract::UniqueRenamer renamer;
  REQUIRE(renamer.apply(*d.cell, extract::UniqueMode::all, d.context(false)).ok());
  const extract::NodeRegionList nodes = nodes_of(*d.cell, *d.t, d.style);
  CHECK(nodes[0].labels.at(0).text == "out");
  CHECK(nodes[0].labels.at(0).port);
  CHECK(nodes[1].labels.at(0).text == "out_uq0");
}

TEST_CASE("A renamed port takes the next port number and gets its number back on revert",
          "[cap:extract-nodes-and-labels][req:Unique naming of duplicate labels]") {
  Duplicates d;
  label(*d.cell, *d.t, "p", "metal1", {5, 5, 5, 5}, 3);
  label(*d.cell, *d.t, "p", "metal1", {25, 5, 25, 5}, 3);
  extract::UniqueRenamer renamer;
  REQUIRE(renamer.apply(*d.cell, extract::UniqueMode::all, d.context(true)).ok());
  const db::Label& moved = d.cell->labels().back();
  CHECK(moved.text == "p_uq0");
  CHECK(moved.port_index == 4);
  renamer.revert();
  CHECK(d.cell->labels().back().text == "p");
  CHECK(d.cell->labels().back().port_index == 3);
}

TEST_CASE("The suffix skips names the cell already has",
          "[cap:extract-nodes-and-labels][req:Unique naming of duplicate labels]") {
  Duplicates d;
  paint(*d.cell, *d.t, "metal1", {40, 0, 50, 10});
  label(*d.cell, *d.t, "a", "metal1", {5, 5, 5, 5});
  label(*d.cell, *d.t, "a", "metal1", {25, 5, 25, 5});
  label(*d.cell, *d.t, "a_uq0", "metal1", {45, 5, 45, 5});
  extract::UniqueRenamer renamer;
  REQUIRE(renamer.apply(*d.cell, extract::UniqueMode::all, d.context(false)).ok());
  std::vector<std::string> texts = d.texts();
  std::sort(texts.begin(), texts.end());
  CHECK(texts == std::vector<std::string>{"a", "a_uq0", "a_uq1"});
}

TEST_CASE("Tagged mode leaves global names alone and warns about other names",
          "[cap:extract-nodes-and-labels][req:Unique naming of duplicate labels]") {
  Duplicates d;
  label(*d.cell, *d.t, "vdd!", "metal1", {5, 5, 5, 5});
  label(*d.cell, *d.t, "vdd!", "metal1", {25, 5, 25, 5});
  diag::CollectingSink sink;
  extract::UniqueContext ctx = d.context(false);
  ctx.sink = &sink;
  extract::UniqueRenamer renamer;
  REQUIRE(renamer.apply(*d.cell, extract::UniqueMode::tagged, ctx).ok());
  CHECK(d.texts() == std::vector<std::string>{"vdd!", "vdd!"});
  CHECK(renamer.warnings() == 0);
  CHECK(renamer.feedback().empty());

  label(*d.cell, *d.t, "sig", "metal1", {6, 6, 6, 6});
  label(*d.cell, *d.t, "sig", "metal1", {26, 6, 26, 6});
  REQUIRE(renamer.apply(*d.cell, extract::UniqueMode::tagged, ctx).ok());
  CHECK(renamer.warnings() == 2);
  REQUIRE(renamer.feedback().size() == 2);
  CHECK(renamer.feedback()[0].text.starts_with("Non-global label \"sig\" attached to more than one unconnected node: "));
  const auto messages = sink.take();
  REQUIRE(messages.size() == 3);
  CHECK(messages[0].text == "Processing top");
  CHECK(messages[2].text == "top: 2 warnings");
}

TEST_CASE("Noports mode renames only non-port labels and abstract views are skipped",
          "[cap:extract-nodes-and-labels][req:Unique naming of duplicate labels]") {
  Duplicates d;
  label(*d.cell, *d.t, "q", "metal1", {5, 5, 5, 5}, 1);
  label(*d.cell, *d.t, "q", "metal1", {25, 5, 25, 5}, 2);
  extract::UniqueRenamer renamer;
  REQUIRE(renamer.apply(*d.cell, extract::UniqueMode::noports, d.context(false)).ok());
  CHECK(d.texts() == std::vector<std::string>{"q", "q"});
  CHECK(renamer.warnings() == 0);

  d.cell->properties().put("LEFview", std::string{"TRUE"});
  REQUIRE(renamer.apply(*d.cell, extract::UniqueMode::all, d.context(false)).ok());
  CHECK(d.texts() == std::vector<std::string>{"q", "q"});
}

TEST_CASE("Without a style the renamer reports EXTRACT-NODE-004",
          "[cap:extract-nodes-and-labels][req:Unique naming of duplicate labels]") {
  Duplicates d;
  extract::UniqueRenamer renamer;
  const diag::Status status = renamer.apply(*d.cell, extract::UniqueMode::all);
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == "EXTRACT-NODE-004");
}

}  // namespace
}  // namespace hefesto::test::extnode
