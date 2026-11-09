// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Magic's default outline fonts (see hefesto/cell/fonts.hpp).

#include "hefesto/cell/fonts.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "hefesto/cell/diag_codes.hpp"

namespace hefesto::cell {

namespace {

// The first existing `directory / file` (`file` alone when `directories` is empty).
std::optional<platform::Path> find_font(platform::FileSystem& fs, const std::vector<platform::Path>& directories,
                                        const platform::Path& file) {
  for (const platform::Path& dir : directories) {
    const platform::Path path = dir.empty() ? file : dir / file;
    if (fs.exists(path)) {
      return path;
    }
  }
  return std::nullopt;
}

diag::Outcome<display::OutlineFont> read_font(platform::FileSystem& fs, const platform::Path& path, double scale) {
  auto text = fs.read(path);
  if (!text) {
    return text.error();
  }
  return display::parse_outline_font(text.value(), path.filename().string(), scale);
}

}  // namespace

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
    const auto path = find_font(fs, directories, std::string{name} + ".pt3");
    if (!path) {
      continue;
    }
    auto font = read_font(fs, *path, scale);
    if (!font) {
      sink.emit(font.error());
      continue;
    }
    fonts.fonts.push_back(std::make_shared<const display::OutlineFont>(std::move(font).value()));
    ++added;
  }
  return added;
}

diag::Outcome<int> load_font_file(platform::FileSystem& fs, const std::vector<platform::Path>& directories,
                                  std::string_view file, double scale, display::FontRegistry& fonts) {
  const platform::Path name{std::string{file}};
  std::vector<platform::Path> candidates{platform::Path{}};  // as given first
  if (name.is_relative()) {
    candidates.insert(candidates.end(), directories.begin(), directories.end());
  }
  const auto path = find_font(fs, candidates, name);
  if (!path) {
    return diag::make(codes::kNotFound, "font file \"" + std::string{file} + "\" not found");
  }
  auto font = read_font(fs, *path, scale);
  if (!font) {
    return font.error();
  }
  fonts.fonts.push_back(std::make_shared<const display::OutlineFont>(std::move(font).value()));
  return static_cast<int>(fonts.fonts.size()) - 1;
}

}  // namespace hefesto::cell
