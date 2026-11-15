// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// gds-write "Verbatim copy of read-only cells" (Q-MASK-216), "Full dump of
// a vendor library" (Q-MASK-211, 212, 213, 214, 215), "Addendum mode" and
// "Write result and error propagation", against vendor GDS files built in
// the fixture's memory file system.
// Owner: GDSW.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <regex>

#include "write_fixture.hpp"

namespace gds_write_test {
namespace {

// A vendor library built record by record, with the offsets a read-only
// read would record.
class Library {
 public:
  Library() : writer_(sink_) {
    i2(gds::RecordType::header, {600});
    i2(gds::RecordType::bgnlib, std::vector<std::int16_t>(12, 1));
    str(gds::RecordType::libname, "vendor");
    units(1e-9);
  }

  void units(double mpd) {
    const std::array<double, 2> u{0.001, mpd};
    REQUIRE(writer_.write_real8(gds::RecordType::units, u).ok());
  }
  void i2(gds::RecordType t, const std::vector<std::int16_t>& v) { REQUIRE(writer_.write_int2(t, v).ok()); }
  void str(gds::RecordType t, std::string_view s) { REQUIRE(writer_.write_string(t, s).ok()); }
  void empty(gds::RecordType t) { REQUIRE(writer_.write_empty(t).ok()); }

  /// BGNSTR (creation year `year`), STRNAME; returns GDS_START.
  std::uint64_t begin(std::string_view name, std::int16_t year = 99) {
    i2(gds::RecordType::bgnstr, {year, 2, 3, 4, 5, 6, 99, 2, 3, 4, 5, 6});
    str(gds::RecordType::strname, name);
    return position();
  }
  void boundary(std::int16_t layer) {
    empty(gds::RecordType::boundary);
    i2(gds::RecordType::layer, {layer});
    i2(gds::RecordType::datatype, {0});
    const std::array<std::int32_t, 10> xy{0, 0, 10, 0, 10, 10, 0, 10, 0, 0};
    REQUIRE(writer_.write_int4(gds::RecordType::xy, xy).ok());
    empty(gds::RecordType::endel);
  }
  void sref(std::string_view name) {
    empty(gds::RecordType::sref);
    str(gds::RecordType::sname, name);
    const std::array<std::int32_t, 2> xy{0, 0};
    REQUIRE(writer_.write_int4(gds::RecordType::xy, xy).ok());
    empty(gds::RecordType::endel);
  }
  /// ENDSTR; returns GDS_END.
  std::uint64_t end() {
    empty(gds::RecordType::endstr);
    return position();
  }
  std::string bytes() {
    REQUIRE(writer_.flush().ok());
    std::string out;
    for (const std::byte b : sink_.bytes()) {
      out.push_back(static_cast<char>(b));
    }
    return out;
  }
  std::uint64_t position() {
    REQUIRE(writer_.flush().ok());
    return sink_.position();
  }

 private:
  gds::MemorySink sink_;
  gds::RecordWriter writer_;
};

void make_read_only(db::CellDef& def, const std::string& file, std::optional<std::uint64_t> start,
                    std::optional<std::uint64_t> end, std::string_view start_name = "GDS_START") {
  def.properties().put("GDS_FILE", file);
  if (start) {
    def.properties().put(start_name, std::to_string(*start));
  }
  if (end) {
    def.properties().put("GDS_END", std::to_string(*end));
  }
}

std::size_t count(const cmd::CommandResult& result, std::string_view code) {
  return static_cast<std::size_t>(
      std::count_if(result.diagnostics.begin(), result.diagnostics.end(), [&](const auto& d) { return d.code == code; }));
}

std::vector<std::string> snames(const std::vector<Rec>& recs) {
  std::vector<std::string> out;
  for (const Rec& r : recs) {
    if (r.record() == gds::RecordType::sname) {
      out.push_back(r.text());
    }
  }
  return out;
}

// The vendor file of the copy tests: structure sc_inv (one boundary on
// layer 7), its offsets.
struct VendorCopy {
  std::string bytes;
  std::uint64_t bgnstr = 0;
  std::uint64_t start = 0;
  std::uint64_t end = 0;
};

VendorCopy sc_inv_library() {
  Library lib;
  VendorCopy v;
  v.bgnstr = lib.position();
  v.start = lib.begin("sc_inv");
  lib.boundary(7);
  v.end = lib.end();
  lib.empty(gds::RecordType::endlib);
  v.bytes = lib.bytes();
  return v;
}

}  // namespace

TEST_CASE("a new-style vendor copy writes a fresh header and the bytes verbatim",
          "[cap:gds-write][req:Verbatim copy of read-only cells]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    const VendorCopy v = sc_inv_library();
    REQUIRE(f.fs.write_atomic("lib.gds", v.bytes).ok());
    db::CellDef& top = f.cell("top");
    db::CellDef& inv = f.cell("sc_inv");
    f.paint(inv, f.m1, db::Rect{0, 0, 2, 2});  // the representational layout is not written
    make_read_only(inv, "lib.gds", v.start, v.end);
    f.place(top, inv);
    const std::string out = f.write(top);
    REQUIRE(f.last.ok());
    CHECK(count(f.last, "GDS-WRITE-040") == 1);
    const std::string copied = v.bytes.substr(v.start, v.end - v.start);
    const std::size_t at = out.find(copied);
    REQUIRE(at != std::string::npos);
    const std::vector<Rec> before = decode(out.substr(0, at));
    REQUIRE(before.size() >= 2);
    CHECK(before[before.size() - 2].record() == gds::RecordType::bgnstr);
    CHECK(before.back().text() == "sc_inv");
    CHECK(structure_names(decode(out)) == std::vector<std::string>{"sc_inv", "top"});
  }
}

TEST_CASE("an old-style vendor copy writes only the bytes from GDS_BEGIN",
          "[cap:gds-write][req:Verbatim copy of read-only cells]") {
  Fixture f;
  const VendorCopy v = sc_inv_library();
  REQUIRE(f.fs.write_atomic("lib.gds", v.bytes).ok());
  db::CellDef& top = f.cell("top");
  db::CellDef& inv = f.cell("sc_inv");
  make_read_only(inv, "lib.gds", v.bgnstr, v.end, "GDS_BEGIN");
  inv.properties().put("GDS_START", std::to_string(v.start));
  f.place(top, inv);
  const std::string out = f.write(top);
  REQUIRE(f.last.ok());
  const std::string copied = v.bytes.substr(v.bgnstr, v.end - v.bgnstr);
  CHECK(out.find(copied) != std::string::npos);
  CHECK(structure_names(decode(out)) == std::vector<std::string>{"sc_inv", "top"});
}

TEST_CASE("a bad end marker falls back to the layout", "[cap:gds-write][req:Verbatim copy of read-only cells]") {
  Fixture f;
  const VendorCopy v = sc_inv_library();
  REQUIRE(f.fs.write_atomic("lib.gds", v.bytes).ok());
  db::CellDef& top = f.cell("top");
  db::CellDef& inv = f.cell("sc_inv");
  f.paint(inv, f.m1, db::Rect{0, 0, 2, 2});
  make_read_only(inv, "lib.gds", v.start, v.end + 2);
  f.place(top, inv);
  const std::vector<Rec> recs = decode(f.write(top));
  CHECK(f.last.ok());
  REQUIRE(count(f.last, "GDS-WRITE-042") == 1);
  const std::vector<Rec> generated = structure(recs, "sc_inv");
  const auto layer = std::find_if(generated.begin(), generated.end(),
                                  [](const Rec& r) { return r.record() == gds::RecordType::layer; });
  REQUIRE(layer != generated.end());
  CHECK(layer->i2().front() == 49);  // generated on M1, not the vendor's layer 7
}

TEST_CASE("the sanity checks of a vendor copy", "[cap:gds-write][req:Verbatim copy of read-only cells]") {
  const VendorCopy v = sc_inv_library();
  struct Case {
    std::int64_t start;
    std::int64_t end;
  };
  const std::int64_t s = static_cast<std::int64_t>(v.start);
  const std::int64_t e = static_cast<std::int64_t>(v.end);
  // too short, past the end of the file, a start that is not after a
  // STRNAME, a negative start and huge offsets
  for (const Case c : {Case{s, s + 2}, Case{s, e + 1000}, Case{s + 4, e}, Case{-8, e},
                       Case{INT64_MAX - 2, INT64_MAX}}) {
    Fixture f;
    REQUIRE(f.fs.write_atomic("lib.gds", v.bytes).ok());
    db::CellDef& top = f.cell("top");
    db::CellDef& inv = f.cell("sc_inv");
    inv.properties().put("GDS_FILE", std::string{"lib.gds"});
    inv.properties().put("GDS_START", std::to_string(c.start));
    inv.properties().put("GDS_END", std::to_string(c.end));
    f.place(top, inv);
    const std::vector<Rec> recs = decode(f.write(top));
    CHECK(f.last.ok());
    CHECK(count(f.last, "GDS-WRITE-042") == 1);
    CHECK(structure_names(recs) == std::vector<std::string>{"sc_inv", "top"});
  }
}

TEST_CASE("a renamed vendor structure keeps the cell name with a warning",
          "[cap:gds-write][req:Verbatim copy of read-only cells]") {
  Fixture f;
  const VendorCopy v = sc_inv_library();
  REQUIRE(f.fs.write_atomic("lib.gds", v.bytes).ok());
  db::CellDef& top = f.cell("top");
  db::CellDef& inv = f.cell("inverter");
  make_read_only(inv, "lib.gds", v.start, v.end);
  f.place(top, inv);
  const std::vector<Rec> recs = decode(f.write(top));
  CHECK(f.last.ok());
  CHECK(count(f.last, "GDS-WRITE-043") == 1);
  CHECK(structure_names(recs) == std::vector<std::string>{"inverter", "top"});
}

TEST_CASE("a missing vendor file fails the write unless undefined is allow",
          "[cap:gds-write][req:Verbatim copy of read-only cells]") {
  for (const bool allow : {false, true}) {
    Fixture f;
    f.options.settings.allow_undefined = allow;
    db::CellDef& top = f.cell("top");
    db::CellDef& inv = f.cell("sc_inv");
    make_read_only(inv, "missing.gds", 100, 200);
    f.place(top, inv);
    const std::vector<Rec> recs = decode(f.write(top));
    CHECK(f.last.ok() == allow);
    CHECK(count(f.last, "GDS-WRITE-041") == 1);
    CHECK(recs.back().record() == gds::RecordType::endlib);
    CHECK(structure_names(recs) == (allow ? std::vector<std::string>{"top"} : std::vector<std::string>{}));
  }
}

TEST_CASE("a missing vendor file inside a vendor cell is skipped silently",
          "[cap:gds-write][req:Verbatim copy of read-only cells]") {
  Fixture f;
  const VendorCopy v = sc_inv_library();
  REQUIRE(f.fs.write_atomic("lib.gds", v.bytes).ok());
  db::CellDef& top = f.cell("top");
  db::CellDef& outer = f.cell("sc_inv");
  db::CellDef& inner = f.cell("inner");
  make_read_only(outer, "lib.gds", v.start, v.end);
  make_read_only(inner, "gone.gds", 100, 200);
  f.place(outer, inner);
  f.place(top, outer);
  const std::vector<Rec> recs = decode(f.write(top));
  CHECK(f.last.ok());
  CHECK(count(f.last, "GDS-WRITE-041") == 0);
  CHECK(structure_names(recs) == std::vector<std::string>{"sc_inv", "top"});
}

TEST_CASE("copied SNAMEs: verbatim with a warning ON, rewritten OFF (Q-MASK-216)",
          "[cap:gds-write][req:Verbatim copy of read-only cells]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    Library lib;
    const std::uint64_t start = lib.begin("wrap");
    lib.sref("a-b");
    const std::uint64_t end = lib.end();
    lib.empty(gds::RecordType::endlib);
    REQUIRE(f.fs.write_atomic("lib.gds", lib.bytes()).ok());
    db::CellDef& top = f.cell("top");
    db::CellDef& wrap = f.cell("wrap");
    db::CellDef& child = f.cell("a-b");  // written as a_b
    f.paint(child, f.m1, db::Rect{0, 0, 1, 1});
    make_read_only(wrap, "lib.gds", start, end);
    f.place(wrap, child);
    f.place(top, wrap);
    const std::vector<Rec> recs = decode(f.write(top));
    CHECK(f.last.ok());
    CHECK(snames(structure(recs, "wrap")) == std::vector<std::string>{on ? "a-b" : "a_b"});
    CHECK(count(f.last, "GDS-WRITE-045") == (on ? 1U : 0U));
  }
}

TEST_CASE("addendum yes omits vendor structures and keeps the references", "[cap:gds-write][req:Addendum mode]") {
  Fixture f;
  f.options.settings.addendum = true;
  db::CellDef& top = f.cell("top");
  db::CellDef& inv = f.cell("sc_inv");
  make_read_only(inv, "missing.gds", 100, 200);  // never opened
  f.place(top, inv);
  const std::vector<Rec> recs = decode(f.write(top));
  CHECK(f.last.ok());
  CHECK(structure_names(recs) == std::vector<std::string>{"top"});
  CHECK(snames(recs) == std::vector<std::string>{"sc_inv"});
}

namespace {

// pad (abstract, GDS_START only) from a library defining via_x and pad,
// pad referencing via_x; `forward` puts pad first.
std::string pad_library(bool forward, double mpd = 1e-9) {
  Library lib;
  const auto via = [&lib] {
    lib.begin("via_x");
    lib.boundary(3);
    lib.end();
  };
  const auto pad = [&lib] {
    lib.begin("pad");
    lib.sref("via_x");
    lib.end();
  };
  if (forward) {
    pad();
    via();
  } else {
    via();
    pad();
  }
  lib.empty(gds::RecordType::endlib);
  std::string bytes = lib.bytes();
  if (mpd != 1e-9) {
    // UNITS is the fourth record: HEADER (6) + BGNLIB (28) + LIBNAME (10).
    const auto mpd_bytes = gds::encode_real8(mpd);
    for (std::size_t i = 0; i < 8; ++i) {
      bytes[6 + 28 + 10 + 4 + 8 + i] = static_cast<char>(mpd_bytes[i]);
    }
  }
  return bytes;
}

db::CellDef& abstract_pad(Fixture& f, const std::string& name = "pad", std::string view = "TRUE") {
  db::CellDef& pad = f.cell(name);
  pad.properties().put("LEFview", std::move(view));
  pad.properties().put("GDS_FILE", std::string{"lib.gds"});
  pad.properties().put("GDS_START", std::string{"0"});
  return pad;
}

}  // namespace

TEST_CASE("a full dump prefixes the library's other structures", "[cap:gds-write][req:Full dump of a vendor library]") {
  for (const bool on : {true, false}) {
    for (const bool forward : {false, true}) {
      Fixture f(on);
      REQUIRE(f.fs.write_atomic("lib.gds", pad_library(forward)).ok());
      db::CellDef& top = f.cell("top");
      f.place(top, abstract_pad(f));
      const std::vector<Rec> recs = decode(f.write(top));
      CHECK(f.last.ok());
      const std::vector<std::string> names = structure_names(recs);
      REQUIRE(names.size() == 3);
      const std::string via = names[forward ? 1 : 0];
      CHECK(std::regex_match(via, std::regex("[A-Z][A-Z0-9]_via_x")));
      CHECK(names[forward ? 0 : 1] == "pad");
      CHECK(names[2] == "top");
      CHECK(snames(structure(recs, "pad")) == std::vector<std::string>{via});
      CHECK(count(f.last, "GDS-WRITE-047") == 2);
      CHECK(count(f.last, "GDS-WRITE-048") == (forward ? 1U : 0U));
    }
  }
}

TEST_CASE("the full-dump prefix follows the shared random() with magic-compat ON (Q-MASK-211)",
          "[cap:gds-write][req:Full dump of a vendor library]") {
  // glibc random() with seed 1: 1804289383 % 26 = 13 ('N'), 846930886 % 36 = 34 ('8').
  Fixture f(true);
  REQUIRE(f.fs.write_atomic("lib.gds", pad_library(false)).ok());
  db::CellDef& top = f.cell("top");
  f.place(top, abstract_pad(f));
  CHECK(structure_names(decode(f.write(top))).front() == "N8_via_x");

  // OFF: the prefix depends only on the file name.
  std::string first;
  for (int i = 0; i < 2; ++i) {
    Fixture off(false);
    REQUIRE(off.fs.write_atomic("lib.gds", pad_library(false)).ok());
    off.ed.mask.random.seed(static_cast<std::uint32_t>(7 + i));
    db::CellDef& t = off.cell("top");
    off.place(t, abstract_pad(off));
    const std::string name = structure_names(decode(off.write(t))).front();
    if (i == 0) {
      first = name;
    }
    CHECK(name == first);
  }
}

TEST_CASE("the emit phase draws from the write's copy of random(); finish_write stores it back",
          "[cap:gds-write][req:Full dump of a vendor library]") {
  // Regression: the emit phase advanced EditorState::mask.random under the
  // shared lock, a data race between two concurrent writes (D4).
  Fixture f(true);
  REQUIRE(f.fs.write_atomic("lib.gds", pad_library(false)).ok());
  db::CellDef& top = f.cell("top");
  f.place(top, abstract_pad(f));
  f.install();
  hefesto::util::LibcRandom expected;  // seed 1, as the session's
  (void)expected.next();
  (void)expected.next();
  auto prepared = gds::prepare_write(f.ed, top, f.options);
  REQUIRE(prepared.ok());
  const hefesto::util::LibcRandom before = f.ed.mask.random;
  const cmd::CommandResult result = gds::emit_write(prepared.value(), "/out.gds");
  CHECK(result.ok());
  hefesto::util::LibcRandom session = f.ed.mask.random;
  hefesto::util::LibcRandom untouched = before;
  CHECK(session.next() == untouched.next());  // emit left the session's state alone
  gds::finish_write(f.ed, prepared.value());
  CHECK(f.ed.mask.random.next() == expected.next());  // two draws were made and stored back
  CHECK(structure_names(decode(f.file())).front() == "N8_via_x");
}

TEST_CASE("LEFview no_prefix dumps without a prefix", "[cap:gds-write][req:Full dump of a vendor library]") {
  Fixture f;
  REQUIRE(f.fs.write_atomic("lib.gds", pad_library(false)).ok());
  db::CellDef& top = f.cell("top");
  f.place(top, abstract_pad(f, "pad", "No_Prefix"));
  const std::vector<Rec> recs = decode(f.write(top));
  CHECK(structure_names(recs) == std::vector<std::string>{"via_x", "pad", "top"});
  CHECK(snames(structure(recs, "pad")) == std::vector<std::string>{"via_x"});
}

TEST_CASE("a library is dumped once per write", "[cap:gds-write][req:Full dump of a vendor library]") {
  Fixture f;
  REQUIRE(f.fs.write_atomic("lib.gds", pad_library(false)).ok());
  db::CellDef& top = f.cell("top");
  f.place(top, abstract_pad(f, "pad"));
  db::CellDef& via = abstract_pad(f, "via_x");
  f.place(top, via, db::translation(20, 0));
  const std::vector<Rec> recs = decode(f.write(top));
  CHECK(f.last.ok());
  // via_x is an abstract view of the same library, so it keeps its name.
  CHECK(structure_names(recs) == std::vector<std::string>{"via_x", "pad", "top"});
}

TEST_CASE("incompatible library units refuse the dump once (Q-MASK-212)",
          "[cap:gds-write][req:Full dump of a vendor library]") {
  for (const bool allow : {false, true}) {
    Fixture f;
    f.options.settings.allow_undefined = allow;
    REQUIRE(f.fs.write_atomic("lib.gds", pad_library(false, 1e-8)).ok());
    db::CellDef& top = f.cell("top");
    f.place(top, abstract_pad(f, "pad"));
    f.place(top, abstract_pad(f, "pad2"), db::translation(20, 0));
    const std::vector<Rec> recs = decode(f.write(top));
    CHECK(f.last.ok() == allow);
    CHECK(count(f.last, "GDS-WRITE-046") == 1);
    CHECK(structure_names(recs) == (allow ? std::vector<std::string>{"top"} : std::vector<std::string>{}));
  }
}

TEST_CASE("an empty vendor library writes nothing (Q-MASK-213)", "[cap:gds-write][req:Full dump of a vendor library]") {
  Fixture f;
  Library lib;
  lib.empty(gds::RecordType::endlib);
  REQUIRE(f.fs.write_atomic("lib.gds", lib.bytes()).ok());
  db::CellDef& top = f.cell("top");
  f.place(top, abstract_pad(f));
  const std::vector<Rec> recs = decode(f.write(top));
  CHECK(f.last.ok());
  CHECK(structure_names(recs) == std::vector<std::string>{"top"});
  CHECK(std::count_if(recs.begin(), recs.end(), [](const Rec& r) { return r.record() == gds::RecordType::endlib; }) ==
        1);
}

TEST_CASE("an undefined forward reference is appended at the end of the write (Q-MASK-214)",
          "[cap:gds-write][req:Full dump of a vendor library]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    Library lib;
    lib.begin("pad");
    lib.sref("ext");
    lib.end();
    lib.empty(gds::RecordType::endlib);
    REQUIRE(f.fs.write_atomic("lib.gds", lib.bytes()).ok());
    db::CellDef& top = f.cell("top");
    f.place(top, abstract_pad(f));
    db::CellDef& ext = f.cell("ext");
    f.paint(ext, f.m1, db::Rect{0, 0, 1, 1});
    const std::vector<Rec> recs = decode(f.write(top));
    CHECK(f.last.ok());
    const std::vector<std::string> names = structure_names(recs);
    REQUIRE(names.size() == 3);
    const std::string reference = snames(structure(recs, "pad")).front();
    CHECK(std::regex_match(reference, std::regex("[A-Z][A-Z0-9]_ext")));
    CHECK(names[2] == (on ? "ext" : reference));
    CHECK(count(f.last, "GDS-WRITE-050") == (on ? 1U : 0U));
  }
}

TEST_CASE("full-dump creation dates (Q-MASK-215)", "[cap:gds-write][req:Full dump of a vendor library]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    f.options.settings.datestamp.reset();
    REQUIRE(f.fs.write_atomic("lib.gds", pad_library(false)).ok());
    db::CellDef& top = f.cell("top");
    db::CellDef& pad = abstract_pad(f);
    pad.set_timestamp(0);
    f.place(top, pad);
    const std::vector<Rec> recs = decode(f.write(top));
    const std::vector<Rec> via = structure(recs, structure_names(recs).front());
    REQUIRE_FALSE(via.empty());
    // The source BGNSTR says year 99; the cell timestamp 0 is 1970.
    CHECK(via.front().i2().front() == (on ? 70 : 99));
  }
}

TEST_CASE("a failed write still terminates the stream", "[cap:gds-write][req:Write result and error propagation]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& top = f.cell("top");
    db::CellDef& inv = f.cell("sc_inv");
    make_read_only(inv, "missing.gds", 100, 200);
    f.place(top, inv);
    const std::string out = f.write(top);
    CHECK_FALSE(f.last.ok());
    CHECK(f.has_code("GDS-WRITE-010"));
    REQUIRE(out.size() >= 4);
    CHECK(bytes_of(out.substr(out.size() - 4)) == Bytes{0x00, 0x04, 0x04, 0x00});
  }
}

TEST_CASE("a compressed vendor file is copied from its decompressed bytes",
          "[cap:gds-write][req:Verbatim copy of read-only cells]") {
  Fixture f;
  const VendorCopy v = sc_inv_library();
  {
    auto sink = gds::open_sink(f.fs, "lib.gds.gz", 6);
    REQUIRE(sink.ok());
    std::vector<std::byte> raw(v.bytes.size());
    std::transform(v.bytes.begin(), v.bytes.end(), raw.begin(), [](char c) { return static_cast<std::byte>(c); });
    REQUIRE(sink.value()->write(raw).ok());
    REQUIRE(sink.value()->finish().ok());
  }
  db::CellDef& top = f.cell("top");
  db::CellDef& inv = f.cell("sc_inv");
  make_read_only(inv, "lib.gds", v.start, v.end);
  f.place(top, inv);
  const std::string out = f.write(top);
  REQUIRE(f.last.ok());
  CHECK(out.find(v.bytes.substr(v.start, v.end - v.start)) != std::string::npos);
}

}  // namespace gds_write_test
