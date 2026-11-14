// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Output style selection (tasks.md 3.4): exact name, unique prefix,
// ambiguity and unknown names, the lambda rescale of the selected style,
// and the load messages Magic prints when `cif ostyle` re-reads a style.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "style_fixture.hpp"

namespace mask = hefesto::mask;
namespace diag = hefesto::diag;
using mask::test::compat_mode;
using mask::test::read;

TEST_CASE("a unique prefix selects a style", "[cap:mask-output-styles][req:Output style selection]") {
  // Scenario "Unique prefix".
  auto r = read("style gdsii\nscalefactor 1\nlayer A metal1\nstyle cif\nscalefactor 2\nlayer B metal1");
  REQUIRE(mask::select_output_style(r.set, "cif", compat_mode(true)).ok());
  CHECK(r.set.current_style()->name == "cif");
  REQUIRE(mask::select_output_style(r.set, "gd", compat_mode(true)).ok());
  CHECK(r.set.current_style()->name == "gdsii");
  CHECK(r.set.current == std::size_t{0});
}

TEST_CASE("an ambiguous prefix leaves the current style", "[cap:mask-output-styles][req:Output style selection]") {
  // Scenario "Ambiguous prefix".
  for (const bool compat : {true, false}) {
    auto r = read("style sky130 variant (vendor),(fill)\nscalefactor 1\nlayer A metal1");
    REQUIRE(r.set.current == std::size_t{0});
    const auto st = mask::select_output_style(r.set, "sky130(", compat_mode(compat));
    REQUIRE_FALSE(st.ok());
    CHECK(st.error().code == "MASK-STYLE-005");
    CHECK(st.error().text == "CIF output style \"sky130(\" is ambiguous.");
    CHECK(r.set.current == std::size_t{0});
  }
}

TEST_CASE("an exact name wins over longer names it starts", "[cap:mask-output-styles][req:Output style selection]") {
  auto r = read("style gdsfoo\nscalefactor 1\nlayer A metal1\nstyle gds\nscalefactor 1\nlayer B metal1");
  REQUIRE(mask::select_output_style(r.set, "gds", compat_mode(true)).ok());
  CHECK(r.set.current_style()->name == "gds");
  const auto unknown = mask::select_output_style(r.set, "nope", compat_mode(true));
  REQUIRE_FALSE(unknown.ok());
  CHECK(unknown.error().code == "MASK-STYLE-006");
  CHECK(unknown.error().text == "\"nope\" is not one of the CIF output styles Magic knows.");
  CHECK(r.set.current_style()->name == "gds");
}

TEST_CASE("selection rescales the style by the lambda ratio and reloads it fresh",
          "[cap:mask-output-styles][req:Output style selection]") {
  auto r = read("style a\nscalefactor 10 nanometers\nlayer A metal1\ngrow 50\nbloat-all metal1 metal2\n"
                "style b\nscalefactor 20 nanometers\nlayer B metal1\ngrow 60\nbloat-all metal1 metal2");
  diag::CollectingSink sink;
  // lambda 2 : 1 (DBLambda): n = 2, d = 1.
  REQUIRE(mask::select_output_style(r.set, "b", 2, 1, compat_mode(true), sink).ok());
  CHECK(r.set.current_style()->scale.scale_factor == 40);
  CHECK(r.set.current_style()->scale.expander == 10);
  CHECK(r.set.current_style()->layers[0].ops[1].distance == 60);
  // Selecting the current style again changes nothing (CIFLoadStyle).
  REQUIRE(mask::select_output_style(r.set, "b", 2, 1, compat_mode(true), sink).ok());
  CHECK(r.set.current_style()->scale.scale_factor == 40);
  // Back and forth: each selection starts from the style as read.
  REQUIRE(mask::select_output_style(r.set, "a", 2, 1, compat_mode(true), sink).ok());
  CHECK(r.set.current_style()->scale.scale_factor == 20);
  CHECK(r.set.styles[1]->scale.scale_factor == 20);  // b is as read again
  REQUIRE(mask::select_output_style(r.set, "b", 2, 1, compat_mode(true), sink).ok());
  CHECK(r.set.current_style()->scale.scale_factor == 40);
  CHECK(sink.take().empty());
  // Later grid changes rescale the current style.
  const mask::RescaleStep step = mask::rescale_output_styles(r.set, 1, 2, compat_mode(true));
  CHECK(step.multiplier == 2);
  CHECK(step.reduced == 2);
  CHECK(r.set.current_style()->scale.scale_factor == 20);
  CHECK(r.set.current_style()->scale.expander == 10);
  CHECK(r.set.base[1]->scale.scale_factor == 20);
}

TEST_CASE("selecting a style emits its own load messages", "[cap:mask-output-styles][req:Output style selection]") {
  auto r = read("style a\nscalefactor 1\nlayer A metal1\nstyle b\nscalefactor 1\nlayer B metal1\nfrob");
  CHECK(r.messages.empty());  // the tech load reads style a only
  REQUIRE(r.style(1).load_messages.size() == 1);
  diag::CollectingSink sink;
  REQUIRE(mask::select_output_style(r.set, "b", 1, 1, compat_mode(true), sink).ok());
  CHECK(mask::test::texts_of(sink.take()) == std::vector<std::string>{"Unknown statement \"frob\"."});
}
