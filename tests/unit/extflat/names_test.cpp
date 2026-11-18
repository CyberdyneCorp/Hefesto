// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ext-flattening-names: hierarchical names, their hash, use element names,
// the preference order and global detection (EFname.c), on the private
// helpers of src/core/src/extflat/ and through read_flat. Owner: EXTSUB.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "ext_fixture.hpp"
#include "hier_name.hpp"
#include "text.hpp"

namespace extflat = hefesto::extflat;
namespace detail = hefesto::extflat::detail;
using extflat::test::ExtFixture;
using extflat::test::node_named;
using Strings = std::vector<std::string>;

TEST_CASE("a name splits at every slash and joins a path", "[cap:ext-flattening-names][req:Hierarchical name structure]") {
  const std::string joined = detail::concat_names(detail::name_from_string("x1/x2"), detail::name_from_string("a/b"));
  CHECK(detail::name_components(joined) == Strings{"x1", "x2", "a", "b"});
  CHECK(detail::name_to_string(joined) == "x1/x2/a/b");
  CHECK(detail::name_components(detail::name_from_string("a//b/")) == Strings{"a", "", "b", ""});
}

TEST_CASE("the component hash rotates by four bits", "[cap:ext-flattening-names][req:Component hash]") {
  CHECK(detail::component_hash("ab") == 1650U);
  CHECK(detail::component_hash("") == 0U);
  // Bytes are signed chars, as in Magic's x86_64 build.
  CHECK(detail::component_hash("\xff") == 0xFFFFFFFFU);
}

TEST_CASE("array elements are named [y,x], [x] or [y]", "[cap:ext-flattening-names][req:Instance and array element names]") {
  CHECK(detail::use_component("u1", true, true, 2, 1) == "u1[1,2]");
  CHECK(detail::use_component("bit", true, false, 5, 0) == "bit[5]");
  CHECK(detail::use_component("col", false, true, 0, -3) == "col[-3]");
  CHECK(detail::use_component("plain", false, false, 0, 0) == "plain");
}

TEST_CASE("the preference order of names", "[cap:ext-flattening-names][req:Name preference order]") {
  const auto best = [](std::string_view a, std::string_view b) {
    return detail::prefer_name(detail::name_from_string(a), detail::name_from_string(b));
  };
  CHECK(best("a", "b#"));          // a label beats a generated name
  CHECK_FALSE(best("b#", "a"));
  CHECK(best("bb", "x/a"));        // fewer components
  CHECK(best("b", "a"));           // lexically greater wins a tie
  CHECK_FALSE(best("a", "b"));
  CHECK(best("VSS", "0"));         // "0" loses
  CHECK_FALSE(best("0", "VSS"));
  CHECK(best("ab", "abc"));        // fewer characters
  CHECK(best("a!", "b#"));         // '!' gets no extra preference over a label rule
  CHECK(best("z!", "a!"));         // both global: no label rule, lexical order
  CHECK(best("ab/c", "a/bc"));     // components compared from the root side
  // Regression: an empty leaf is read as ending in NUL, not before the name.
  CHECK(best("a/", "a/b#"));
  CHECK(best("", "x"));            // shorter
}

TEST_CASE("only names listed in globalNets are global", "[cap:ext-flattening-names][req:Global and ground detection]") {
  ExtFixture f;
  f.file("top", "use c u1 1 0 0 0 1 0\nuse c u2 1 0 0 0 1 0\n");
  f.file("c", "node vss! 0 0 0 0 m1\nnode VGND 0 0 1 1 m1\n");
  extflat::ReadOptions options;
  options.global_nets = {"VGND"};
  const auto n = f.read("top", options);
  REQUIRE(n.ok());
  CHECK(node_named(n.value(), "VGND") != nullptr);  // merged and renamed to its leaf
  CHECK(node_named(n.value(), "u1/vss!") != nullptr);  // a trailing '!' alone is not global
  CHECK(node_named(n.value(), "u2/vss!") != nullptr);
}

TEST_CASE("efReadLine tokens: quotes group and vanish, a final backslash is dropped",
          "[cap:ext-flattening-read][req:Line tokenization]") {
  CHECK(detail::tokenize("equiv \"a b\" c") == Strings{"equiv", "a b", "c"});
  CHECK(detail::tokenize("  x\"\"  y\\") == Strings{"x", "y"});
  CHECK(detail::tokenize("\"\" z") == Strings{"", "z"});
  CHECK(detail::tokenize("a\\b") == Strings{"a\\b"});
  CHECK(detail::tokenize("   ").empty());
}

TEST_CASE("keyword lookup: exact match wins over prefixes", "[cap:ext-flattening-read][req:Keyword recognition]") {
  CHECK(detail::lookup_key("resist")->key == detail::Key::resistor);
  CHECK(detail::lookup_key("resistc")->key == detail::Key::resistclass);
  CHECK(detail::lookup_key("Kill")->key == detail::Key::killnode);
  CHECK_FALSE(detail::lookup_key("s").has_value());
  CHECK_FALSE(detail::lookup_key("r").has_value());
  CHECK_FALSE(detail::lookup_key("nodes").has_value());
}

TEST_CASE("C conversions of tokens are total", "[cap:ext-flattening-read][req:Node and substrate records]") {
  CHECK(detail::c_atoi("  -12x") == -12);
  CHECK(detail::c_atoi("abc") == 0);
  CHECK(detail::c_atoi("99999999999999999999") == -1);  // LONG_MAX truncated to int, as glibc
  CHECK(detail::c_atoi("4294967297") == 1);
  CHECK(detail::round_half_up_trunc(-5.5) == -5);
  CHECK(detail::round_half_up_trunc(1e300) == 2147483647);
  CHECK(detail::round_half_up_trunc(-1e300) == -2147483647 - 1);
}
