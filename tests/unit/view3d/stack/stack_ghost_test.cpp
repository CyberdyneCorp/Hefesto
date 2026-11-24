// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-3d-visualization "Interactive 3D inspection" (Q-MISC-85, approved
// 2026-11-24): the ghost classes of the layer stack, which the desktop 3D
// tab draws ghosted by default. Owner: SHELL (M10 round 4).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/view3d/stack/ghost.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"
#include "reference_data.hpp"

namespace diag = hefesto::diag;
namespace platform = hefesto::platform;
namespace tech = hefesto::tech;
namespace view3d = hefesto::view3d;

namespace {

// Wells, an implant plane, a blockage plane holding no residue (a
// non-contact type only), and the conducting planes.
constexpr const char* kSynthetic =
    "tech\n format 35\n ghost\nend\n"
    "planes\n well,w\n implant,imp\n active,a\n metal1,m1\n cut,cu\n metal2,m2\nend\n"
    "types\n well nwell\n well pwell\n implant nimplant\n active ndiffusion,ndiff\n active polysilicon,poly\n"
    " active ndcontact,ndc\n metal1 metal1,m1\n metal1 via1\n cut viablock\n metal2 metal2,m2\nend\n"
    "contact\n ndc ndiff metal1\n via1 metal1 metal2\nend\n"
    "styles\n styletype mos\nend\ncompose\nend\nconnect\nend\ncifoutput\nend\ncifinput\nend\ndrc\nend\nextract\nend\n";

std::shared_ptr<const tech::Technology> load_text(const std::string& text) {
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(text, "/t/ghost.tech", fs, tech::LoadOptions{}, sink);
  for (const diag::Diagnostic& d : sink.take()) UNSCOPED_INFO(d.code << ": " << d.text);
  REQUIRE(loaded.ok());
  return loaded.value();
}

std::shared_ptr<const tech::Technology> load_file(const std::filesystem::path& file) {
  platform::LocalFileSystem files;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology(files, file.string(), tech::LoadOptions{}, sink);
  REQUIRE(loaded.ok());
  return loaded.value();
}

tech::TileType type_of(const tech::Technology& t, std::string_view name) {
  const auto found = t.find_type(name);
  REQUIRE(found.ok());
  return *found.id;
}

// A layout-layer stack with one entry per user type (style 0).
view3d::LayerStack stack_of_all_types(const tech::Technology& t) {
  std::vector<view3d::StackLayer> layers;
  for (std::size_t i = tech::kFirstUserType; i < t.type_count(); ++i) {
    view3d::StackLayer layer;
    layer.name = std::string{t.type_name(static_cast<tech::TileType>(i))};
    layer.type = static_cast<tech::TileType>(i);
    layers.push_back(layer);
  }
  return view3d::LayerStack{view3d::Mode::layout_layer, std::move(layers), true, 0.01};
}

// The home-plane names of the ghosted (or opaque) non-contact user types.
std::set<std::string> planes_where(const tech::Technology& t, bool ghosted) {
  const view3d::LayerStack stack = stack_of_all_types(t);
  const std::vector<bool> flags = view3d::ghosted_by_default(stack, t);
  std::set<std::string> planes;
  for (std::size_t i = 0; i < stack.size(); ++i) {
    const tech::TileType type = *stack.layers()[i].type;
    const auto plane = t.home_plane(type);
    if (flags[i] == ghosted && plane && !t.is_contact(type)) planes.insert(std::string{t.plane_name(*plane)});
  }
  return planes;
}

}  // namespace

TEST_CASE("wells, implants and non-conducting planes are ghosted by default; conducting layers are not",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const auto t = load_text(kSynthetic);
  const tech::PlaneMask conducting = view3d::conducting_planes(*t);
  CHECK(view3d::ghosted_type(*t, type_of(*t, "nwell"), conducting));
  CHECK(view3d::ghosted_type(*t, type_of(*t, "pwell"), conducting));
  CHECK(view3d::ghosted_type(*t, type_of(*t, "nimplant"), conducting));
  // A non-contact type on a plane of its own: no contact has a residue there.
  CHECK(view3d::ghosted_type(*t, type_of(*t, "viablock"), conducting));
  CHECK_FALSE(view3d::ghosted_type(*t, type_of(*t, "ndiff"), conducting));
  CHECK_FALSE(view3d::ghosted_type(*t, type_of(*t, "poly"), conducting));  // on a residue's plane
  CHECK_FALSE(view3d::ghosted_type(*t, type_of(*t, "metal1"), conducting));
  CHECK_FALSE(view3d::ghosted_type(*t, type_of(*t, "metal2"), conducting));
  CHECK_FALSE(view3d::ghosted_type(*t, tech::kSpace, conducting));
}

TEST_CASE("contacts and vias are never ghosted",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const auto t = load_text(kSynthetic);
  const tech::PlaneMask conducting = view3d::conducting_planes(*t);
  CHECK_FALSE(view3d::ghosted_type(*t, type_of(*t, "via1"), conducting));
  CHECK_FALSE(view3d::ghosted_type(*t, type_of(*t, "ndc"), conducting));
  CHECK_FALSE(view3d::ghosted_type(*t, type_of(*t, "via1"), tech::PlaneMask{0}));
}

TEST_CASE("ghosted_by_default flags each stack entry; mask-layer entries are never ghosted",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const auto t = load_text(kSynthetic);
  std::vector<view3d::StackLayer> layers(3);
  layers[0].name = "nwell";
  layers[0].type = type_of(*t, "nwell");
  layers[1].name = "metal1";
  layers[1].type = type_of(*t, "metal1");
  layers[2].name = "nwell";  // the same type in a second style
  layers[2].type = type_of(*t, "nwell");
  layers[2].style = 1;
  const view3d::LayerStack layout{view3d::Mode::layout_layer, layers, true, 0.01};
  CHECK(view3d::ghosted_by_default(layout, *t) == std::vector<bool>{true, false, true});
  // Both magic-compat settings give the same classes.
  const view3d::LayerStack off{view3d::Mode::layout_layer, layers, false, 0.01};
  CHECK(view3d::ghosted_by_default(off, *t) == view3d::ghosted_by_default(layout, *t));
  std::vector<view3d::StackLayer> masks(1);
  masks[0].name = "NWELL";
  masks[0].mask_layer = 0;
  const view3d::LayerStack mask{view3d::Mode::mask_layer, masks, true, 0.01};
  CHECK(view3d::ghosted_by_default(mask, *t) == std::vector<bool>{false});
  CHECK(view3d::ghosted_by_default(view3d::LayerStack{}, *t).empty());
}

TEST_CASE("a technology without contact types ghosts nothing",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  std::string text = kSynthetic;
  const std::string contacts = "contact\n ndc ndiff metal1\n via1 metal1 metal2\nend\n";
  text.replace(text.find(contacts), contacts.size(), "contact\nend\n");
  const auto t = load_text(text);
  CHECK(view3d::conducting_planes(*t) == 0);
  for (const bool g : view3d::ghosted_by_default(stack_of_all_types(*t), *t)) CHECK_FALSE(g);
}

TEST_CASE("sky130A: the dwell, well and comment planes are ghosted; block (ubm), active, locali, metals, caps and "
          "contacts are not",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const auto file = hefesto::test::pinned_sky130a_tech();
  if (!file) {
    SKIP("sky130A.tech not in .cache/pdks");
  }
  const auto t = load_file(*file);
  CHECK(planes_where(*t, true) == std::set<std::string>{"dwell", "well", "comment"});
  const std::set<std::string> opaque = planes_where(*t, false);
  for (const char* plane :
       {"active", "locali", "metal1", "metal2", "metal3", "metal4", "metal5", "cap1", "cap2", "metali", "block"}) {
    CHECK(opaque.count(plane) == 1);
  }
  const tech::PlaneMask conducting = view3d::conducting_planes(*t);
  for (const tech::ContactInfo& c : t->contacts()) CHECK_FALSE(view3d::ghosted_type(*t, c.type, conducting));
  CHECK(view3d::ghosted_type(*t, type_of(*t, "nwell"), conducting));
  CHECK(view3d::ghosted_type(*t, type_of(*t, "dnwell"), conducting));
  CHECK_FALSE(view3d::ghosted_type(*t, type_of(*t, "metal1"), conducting));
}

TEST_CASE("Contact planes stay opaque: gf180mcuD ghosts the dwell, well, poly-fill and comment planes",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const auto file = hefesto::test::pinned_gf180mcud_tech();
  if (!file) {
    SKIP("gf180mcuD.tech not in .cache/pdks");
  }
  const auto t = load_file(*file);
  // gf180mcuD's `poly` plane holds only fillpoly (poly itself is on active,
  // where pc has its residue), so the rule ghosts it too.
  CHECK(planes_where(*t, true) == std::set<std::string>{"dwell", "well", "poly", "comment"});
  CHECK(view3d::ghosted_type(*t, type_of(*t, "fillpoly"), view3d::conducting_planes(*t)));
  const std::set<std::string> opaque = planes_where(*t, false);
  for (const char* plane : {"active", "metal1", "metal2"}) CHECK(opaque.count(plane) == 1);
  const tech::PlaneMask conducting = view3d::conducting_planes(*t);
  // The scenario's layers: only the nwell is ghosted.
  CHECK(view3d::ghosted_type(*t, type_of(*t, "nwell"), conducting));
  for (const char* type : {"pdiff", "poly", "pdc", "metal1"}) {
    CHECK_FALSE(view3d::ghosted_type(*t, type_of(*t, type), conducting));
  }
  for (const tech::ContactInfo& c : t->contacts()) CHECK_FALSE(view3d::ghosted_type(*t, c.type, conducting));
}
