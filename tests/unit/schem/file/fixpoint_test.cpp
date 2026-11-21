// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The round-trip property of design D6: for any input, read -> write ->
// read -> write is a fixpoint (capability schematic-file-format "Round-trip
// guarantee"; openspec/changes/m8-schematic tasks.md 6.4). Inputs are
// random documents written out, and random record soup. The generator is
// seeded, so a failure names the seed that reproduces it.
//
// Coordinates are drawn from values with at most 15 significant digits and
// the special values: `%.16g` ("Numeric fields") is not idempotent for every
// binary64 (a 16-digit decimal near 2^53 reads back as a neighbour that
// prints differently), in xschem as in Hefesto. Header texts start with a
// letter: "Version record and header text" removes leading blanks and then
// one LF on every read, so a header that starts with a blank or an LF (as
// one read can leave it: LF, blank, text) changes again on the next read.
//
// Owner: FORMAT.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <string>

#include "file_test_util.hpp"
#include "hefesto/schem/file/read_write.hpp"

namespace schem = hefesto::schem;

namespace {

class Generator {
 public:
  explicit Generator(std::uint64_t seed) : rng_(seed) {}

  int pick(int n) { return std::uniform_int_distribution<int>{0, n - 1}(rng_); }

  double number() {
    static constexpr std::array<double, 8> kSpecial = {0.0, -0.0, 1e-300, -1e300, 0.1, 2.5e-7,
                                                       std::numeric_limits<double>::infinity(), 12345.678901};
    switch (pick(4)) {
      case 0:
        return kSpecial[static_cast<std::size_t>(pick(static_cast<int>(kSpecial.size())))];
      case 1:
        return static_cast<double>(pick(2001) - 1000) / 2.0;
      case 2:
        return std::numeric_limits<double>::quiet_NaN() * (pick(2) == 0 ? 1.0 : -1.0);
      default:
        return static_cast<double>(pick(1'000'000) - 500'000) * std::pow(10.0, pick(20) - 10);
    }
  }

  std::string text() {
    static constexpr std::string_view kAlphabet = "ab =;\"\\{}\n\r\t[]#@xyz\xc3\xa9";
    std::string out;
    const int length = pick(12);
    for (int i = 0; i < length; ++i) {
      out.push_back(kAlphabet[static_cast<std::size_t>(pick(static_cast<int>(kAlphabet.size())))]);
    }
    return out;
  }

  schem::Point point() { return {number(), number()}; }

  schem::Document document(int depth) {
    schem::Document doc;
    doc.kind = pick(2) == 0 ? schem::FileKind::schematic : schem::FileKind::symbol;
    doc.header_text = pick(3) == 0 ? "h" + text() : "";
    doc.globals = {text(), text(), text(), text(), text(), text()};
    for (int i = pick(6); i > 0; --i) {
      add_record(doc, depth);
    }
    return doc;
  }

 private:
  void add_record(schem::Document& doc, int depth) {
    const int layer = pick(24) - 1;  // -1 and 22 are out of range
    switch (pick(8)) {
      case 0:
        doc.lines.push_back({layer, point(), point(), text()});
        break;
      case 1:
        doc.boxes.push_back({layer, point(), point(), text()});
        break;
      case 2:
        doc.arcs.push_back({layer, point(), number(), number(), number(), text()});
        break;
      case 3:
        add_polygon(doc, layer);
        break;
      case 4:
        doc.texts.push_back({text(), point(), pick(9) - 4, pick(3), number(), number(), text()});
        break;
      case 5:
        doc.wires.push_back({point(), point(), text()});
        break;
      default:
        add_instance(doc, depth);
        break;
    }
  }

  void add_polygon(schem::Document& doc, int layer) {
    schem::PolygonRecord polygon{layer, {}, text()};
    for (int i = pick(5); i > 0; --i) {
      polygon.points.push_back(point());
    }
    doc.polygons.push_back(std::move(polygon));
  }

  void add_instance(schem::Document& doc, int depth) {
    const std::string symbol = "s" + std::to_string(pick(3)) + ".sym";
    doc.instances.push_back({symbol, point(), pick(4), pick(2), pick(2) == 0 ? text() : "embed=true " + text()});
    if (depth > 0 && pick(2) == 0) {
      doc.embedded.push_back({symbol, document(depth - 1)});
    }
  }

  std::mt19937_64 rng_;
};

// Random lines of record tags, numbers, braces and noise.
std::string soup(Generator& g) {
  static constexpr std::array<std::string_view, 24> kTokens = {
      "v ", "K ", "G ", "L ", "B ", "A ", "P ", "T ", "N ", "C ", "[", "]", "# ", "Z ",
      "{", "}", "\\", "\n", "\r\n", " ", "4 ", "-2.5 ", "1e5 ", "x"};
  std::string out;
  for (int i = g.pick(80); i > 0; --i) {
    out += kTokens[static_cast<std::size_t>(g.pick(static_cast<int>(kTokens.size())))];
  }
  return out;
}

void check_fixpoint(const std::string& input, schem::FileKind kind, bool compat) {
  const std::string first = schem::test::round_trip(input, kind, compat);
  const std::string second = schem::test::round_trip(first, kind, compat);
  CHECK(second == first);
}

}  // namespace

TEST_CASE("read, write, read, write is a fixpoint for written documents",
          "[cap:schematic-file-format][req:Round-trip guarantee]") {
  for (std::uint64_t seed = 1; seed <= 3000; ++seed) {
    Generator g{seed};
    const schem::Document doc = g.document(2);
    INFO("seed " << seed);
    check_fixpoint(schem::write_document(doc), doc.kind, g.pick(2) == 0);
  }
}

TEST_CASE("read, write, read, write is a fixpoint for record soup", "[cap:schematic-file-format][req:Round-trip guarantee]") {
  for (std::uint64_t seed = 1; seed <= 3000; ++seed) {
    Generator g{seed};
    const std::string input = soup(g);
    const auto kind = g.pick(2) == 0 ? schem::FileKind::schematic : schem::FileKind::symbol;
    const bool compat = g.pick(2) == 0;
    INFO("seed " << seed);
    const std::string first = schem::test::round_trip(input, kind, compat);
    const std::string second = schem::test::round_trip(first, kind, compat);
    // A header that starts with a blank or an LF changes once per read (see above).
    const std::string header = schem::test::read_text(input, kind, compat).doc.header_text;
    if (header.empty() || header.find_first_of(" \t\n") != 0) {
      CHECK(second == first);
    }
  }
}
