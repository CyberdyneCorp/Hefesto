// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `plot` section state machine (capability plot-output "Technology plot
// section structure", "Raster printer and legacy plot devices"; m10-plot-route
// design D6). Lines go to the handler of the current style; `gremlin`,
// `versatec`, `colorversatec` and `pixels` lines are accepted and ignored
// (Q-MISC-51 (a)).
//
// Ported from Magic VLSI 8.3.694 plot/plotMain.c (PlotTechInit,
// PlotTechLine, PlotTechFinal), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: PLOTTECH.

#include "hefesto/plot/tech/plot_tech.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "plot/tech/reader.hpp"

namespace hefesto::plot {

namespace {

/// Magic's plotStyles[] in table order.
enum class Device : std::uint8_t { postscript, pnm, gremlin, versatec, colorversatec, pixels };

constexpr std::array<std::string_view, 6> kDeviceNames = {"postscript", "pnm",           "gremlin",
                                                           "versatec",   "colorversatec", "pixels"};

std::optional<Device> find_device(std::string_view name) {
  for (std::size_t i = 0; i < kDeviceNames.size(); ++i) {
    if (kDeviceNames[i] == name) {
      return static_cast<Device>(i);
    }
  }
  return std::nullopt;
}

/// Adds the section to every located diagnostic, so the Magic rendering
/// prints TechPrintLine's context ("<file>: line <n>: section plot:").
class SectionSink final : public diag::DiagnosticSink {
 public:
  explicit SectionSink(diag::DiagnosticSink& inner) : inner_(inner) {}

  void emit(const diag::Diagnostic& diagnostic) override {
    const auto* where = std::get_if<diag::SourceLocation>(&diagnostic.location);
    if (where == nullptr || where->line == 0 || diagnostic.related.has_value()) {
      inner_.emit(diagnostic);
      return;
    }
    diag::Diagnostic stamped = diagnostic;
    stamped.related = diag::ObjectRef{diag::ObjectKind::section, "plot"};
    inner_.emit(stamped);
  }

 private:
  diag::DiagnosticSink& inner_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

/// plotCurStyle: no `style` line yet (-1), skipping (-2), or a device.
struct Current {
  bool seen_style = false;
  std::optional<Device> device;  // nullopt while skipping
};

class SectionReader {
 public:
  SectionReader(const tech::Technology& tech, const StyleSource& styles, diag::DiagnosticSink& sink)
      : tech_(tech), sink_(sink), rep_(sink_), pnm_(tech, styles, rep_) {}

  void line(const tech::RawLine& line) {
    rep_.set_location(diag::SourceLocation{line.file, line.line}, "plot");
    const std::span<const std::string> tokens{line.tokens};
    if (tokens.empty()) {
      return;
    }
    if (tokens.front() == "style") {
      style_line(tokens);
      return;
    }
    if (!current_.seen_style) {
      rep_.emit(kStructure, "Must declare a plot style before anything else.");
      current_.seen_style = true;  // plotCurStyle = -2: skip until the next style
      return;
    }
    if (current_.device) {
      device_line(*current_.device, tokens);
    }
  }

  PlotTech finish() {
    pnm_.finish(out_);  // PlotPNMTechFinal, the only final procedure
    out_.has_section = true;
    return std::move(out_);
  }

 private:
  static constexpr std::string_view kStructure = "PLOT-TECH-001";

  void style_line(std::span<const std::string> tokens) {
    if (tokens.size() != 2) {
      // Magic returns without changing the current style.
      rep_.emit(kStructure, "\"style\" lines must have exactly two arguments");
      return;
    }
    current_.seen_style = true;
    current_.device = find_device(tokens[1]);
    if (!current_.device) {
      rep_.emit(kStructure, "Plot style \"" + tokens[1] + "\" doesn't exist.  Ignoring.");
    }
  }

  void device_line(Device device, std::span<const std::string> tokens) {
    switch (device) {
      case Device::postscript:
        detail::postscript_line(tokens, tech_, rep_, out_);
        return;
      case Device::pnm:
        pnm_.line(tokens);
        return;
      case Device::gremlin:
      case Device::versatec:
      case Device::colorversatec:
      case Device::pixels:
        return;  // accepted and ignored (Q-MISC-51 (a); gremlin and pixels as Magic)
    }
  }

  const tech::Technology& tech_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  SectionSink sink_;
  tech::Reporter rep_;
  detail::PnmHandler pnm_;
  Current current_;
  PlotTech out_;
};

}  // namespace

PlotTech read_plot_section(const tech::RawSection* section, const tech::Technology& tech, const StyleSource& styles,
                           diag::DiagnosticSink& sink) {
  if (section == nullptr) {
    return PlotTech{};  // no section: PlotTechInit never runs (PNM defaults at plot time)
  }
  SectionReader reader(tech, styles, sink);
  for (const tech::RawLine& line : section->lines) {
    reader.line(line);
  }
  return reader.finish();
}

}  // namespace hefesto::plot
