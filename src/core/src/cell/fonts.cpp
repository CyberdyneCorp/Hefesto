// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Magic's default outline fonts (see hefesto/cell/fonts.hpp).

#include "hefesto/cell/fonts.hpp"

#include <array>
#include <memory>
#include <string_view>
#include <utility>

namespace hefesto::cell {

std::size_t load_default_fonts(platform::FileSystem& fs, const std::vector<platform::Path>& directories,
                               display::FontRegistry& fonts, diag::DiagnosticSink& sink) {
  // magic/proto.magicrc.in: "Load basic set of fonts".
  static constexpr std::array<std::pair<std::string_view, double>, 3> kDefaults = {
      std::pair{std::string_view{"FreeSans"}, 0.58}, std::pair{std::string_view{"FreeSerif"}, 0.58},
      std::pair{std::string_view{"FreeMono"}, 0.6}};
  std::size_t added = 0;
  for (const auto& [name, scale] : kDefaults) {
    if (fonts.find(name) >= 0) {
      continue;
    }
    const std::string file = std::string{name} + ".pt3";
    for (const platform::Path& dir : directories) {
      const platform::Path path = dir / file;
      if (!fs.exists(path)) {
        continue;
      }
      auto text = fs.read(path);
      auto font = text ? display::parse_outline_font(text.value(), file, scale)
                       : diag::Outcome<display::OutlineFont>{text.error()};
      if (!font) {
        sink.emit(font.error());
        break;
      }
      fonts.fonts.push_back(std::make_shared<const display::OutlineFont>(std::move(font).value()));
      ++added;
      break;
    }
  }
  return added;
}

}  // namespace hefesto::cell
