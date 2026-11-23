// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A small technology and helpers for the ExtStyle tests of
// extract-tech-section (openspec/changes/m7-extraction tasks.md 4.2-4.6):
// load_styles() over a hand-written `extract` section, the parsed and the
// converted style, type and plane lookups. Owner: EXTTECH.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"

namespace hefesto::test::ext {

namespace extract = hefesto::extract;
namespace tech = hefesto::tech;
namespace diag = hefesto::diag;
namespace compat = hefesto::compat;
namespace mask = hefesto::mask;
namespace platform = hefesto::platform;

// Planes well, active, metal1, metal2, metal3 in declaration order; ndc and
// via are contacts (via has residues on metal1 and metal2); `comment` is
// mentioned nowhere and connects to nothing.
inline std::string tech_text(const std::string& extract_body, const std::string& cifoutput_body = "",
                             const std::string& format = " format 35\n") {
  return "tech\n" + format + " exttech\nend\n"
         "planes\n well,w\n active,a\n metal1,m1\n metal2,m2\n metal3,m3\nend\n"
         "types\n well pwell,pw\n well nwell,nw\n active ndiff,nd\n active pdiff,pd\n active psd\n"
         " active poly,p\n active nfet\n active pfet\n active ndc\n metal1 metal1,m1\n metal1 via\n"
         " metal2 metal2,m2\n metal3 metal3,m3\n metal3 comment\nend\n"
         "contact\n ndc ndiff metal1\n via metal1 metal2\nend\n"
         "styles\n styletype mos\nend\n"
         "compose\nend\n"
         "connect\n pwell psd\n ndiff ndc\n metal1 ndc,via\n metal2 via\n poly nfet,pfet\nend\n"
         "cifoutput\n" +
         cifoutput_body + "end\ncifinput\nend\ndrc\nend\nextract\n" + extract_body + "end\n";
}

// Every technology plane ordered in declaration order.
inline const std::string kOrdered =
    " planeorder well 0\n planeorder active 1\n planeorder metal1 2\n planeorder metal2 3\n planeorder metal3 4\n";

// A cifoutput style whose dscale is 0.5 micron per lambda.
inline const std::string kHalfMicronCif = "style lambda\n scalefactor 50\n layer M1 metal1\n calma 1 0\n";

struct Loaded {
  std::shared_ptr<const tech::Technology> tech;
  extract::StyleTable table;
  std::vector<diag::Diagnostic> messages;  // what load_styles reported
};

inline std::shared_ptr<const tech::Technology> load_tech(const std::string& text) {
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(text, "/t/exttech.tech", fs, tech::LoadOptions{}, sink);
  for (const auto& d : sink.snapshot()) {
    UNSCOPED_INFO(d.code << ": " << d.text);
  }
  REQUIRE(loaded.ok());
  return loaded.value();
}

inline Loaded load_full(const std::shared_ptr<const tech::Technology>& t, bool magic_compat = true) {
  Loaded out;
  out.tech = t;
  const tech::RawSection* section = nullptr;
  for (const tech::RawSection& raw : t->raw_sections()) {
    if (raw.name == "extract") {
      section = &raw;
    }
  }
  REQUIRE(section != nullptr);
  diag::CollectingSink sink;
  compat::CompatSettings compat;
  compat.magic_compat = magic_compat;
  out.table = extract::load_styles(*section, *t, sink, compat);
  out.messages = sink.take();
  for (const auto& d : out.messages) {
    UNSCOPED_INFO("load_styles: " << d.code << ": " << d.text);
  }
  return out;
}

inline Loaded load(const std::string& extract_body, bool magic_compat = true, const std::string& cif = "") {
  return load_full(load_tech(tech_text(extract_body, cif)), magic_compat);
}

// The tech-load style (the first one), parsed.
inline extract::ExtStyle parsed(const Loaded& l) {
  REQUIRE_FALSE(l.table.names().empty());
  const auto style = l.table.select(l.table.names().front());
  REQUIRE(style != nullptr);
  return *style;
}

// The tech-load style micron-converted with the technology's first
// cifoutput style, as the hook does at a 1:1 grid.
inline extract::ExtStyle finished(const Loaded& l) {
  const std::optional<mask::OutputStyle> cif = mask::read_first_output_style(*l.tech);
  return parsed(l).micron_convert(cif ? &*cif : nullptr);
}

inline tech::TileType type_of(const tech::Technology& t, std::string_view name) {
  const auto found = t.find_type(name);
  REQUIRE(found.ok());
  return *found.id;
}

inline tech::PlaneId plane_of(const tech::Technology& t, std::string_view name) {
  const auto found = t.find_plane(name);
  REQUIRE(found.ok());
  return *found.id;
}

inline tech::TypeMask types_of(const tech::Technology& t, std::string_view list) {
  const auto parsed_list = t.parse_type_list(list);
  REQUIRE(parsed_list.ok());
  return parsed_list.value();
}

inline std::vector<std::string> codes(const std::vector<diag::Diagnostic>& messages) {
  std::vector<std::string> out;
  for (const auto& d : messages) {
    out.push_back(d.code);
  }
  return out;
}

// The messages other than the non-electrical list and the plane-order
// fallback warning (a style without `planeorder` lines).
inline std::vector<diag::Diagnostic> line_messages(const std::vector<diag::Diagnostic>& messages) {
  std::vector<diag::Diagnostic> out;
  for (const auto& d : messages) {
    if (d.code != "EXTRACT-STYLE-018" && d.code != "EXTRACT-STYLE-006") {
      out.push_back(d);
    }
  }
  return out;
}

}  // namespace hefesto::test::ext
