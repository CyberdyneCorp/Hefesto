// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Writing raw files ("Raw file format", Q-SIM-17) and display names
// ("Final vector retrieval"). Owner: SIMRES. See raw.hpp.

#include <array>
#include <bit>
#include <cstdio>
#include <ctime>

#include "hefesto/sim/result/raw.hpp"

namespace hefesto::sim {

namespace {

void put_le(std::string& out, double value) {
  const auto bits = std::bit_cast<std::uint64_t>(value);
  for (int i = 0; i < 8; ++i) {
    out.push_back(static_cast<char>((bits >> (8 * i)) & 0xFFU));
  }
}

std::string format_e15(double value) {
  std::array<char, 64> buffer{};
  const int n = std::snprintf(buffer.data(), buffer.size(), "%.15e", value);
  return std::string(buffer.data(), static_cast<std::size_t>(n > 0 ? n : 0));
}

std::string now_asctime() {
  const std::time_t t = std::time(nullptr);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &t);
#else
  localtime_r(&t, &local);
#endif
  std::array<char, 64> buffer{};
  const std::size_t n = std::strftime(buffer.data(), buffer.size(), "%a %b %e %H:%M:%S %Y", &local);
  return std::string(buffer.data(), n);
}

bool any_complex(const Plot& plot) {
  for (const Vector& v : plot.vectors) {
    if (v.complex) {
      return true;
    }
  }
  return false;
}

double imag_at(const Vector& v, std::size_t i) { return v.complex && i < v.imag.size() ? v.imag[i] : 0.0; }

void header(std::string& out, const Plot& plot, const RawWriteOptions& options, bool complex) {
  out += "Title: " + options.title + "\n";
  out += "Date: " + (options.date.empty() ? now_asctime() : options.date) + "\n";
  if (!options.command.empty()) {
    out += "Command: " + options.command + "\n";
  }
  out += "Plotname: " + plot.name + "\n";
  out += std::string{"Flags: "} + (complex ? "complex" : "real") + "\n";
  out += "No. Variables: " + std::to_string(plot.vectors.size()) + "\n";
  out += "No. Points: " + std::to_string(plot.vectors.front().real.size()) + "\n";
  out += "Variables:\n";
  for (std::size_t i = 0; i < plot.vectors.size(); ++i) {
    const Vector& v = plot.vectors[i];
    out += "\t" + std::to_string(i) + "\t" + display_name(v) + "\t" + (v.type.empty() ? "notype" : v.type) + "\n";
  }
}

void binary_data(std::string& out, const Plot& plot, bool complex) {
  const std::size_t points = plot.vectors.front().real.size();
  out += "Binary:\n";
  for (std::size_t p = 0; p < points; ++p) {
    for (const Vector& v : plot.vectors) {
      put_le(out, v.real[p]);
      if (complex) {
        put_le(out, imag_at(v, p));
      }
    }
  }
}

void ascii_data(std::string& out, const Plot& plot, bool complex) {
  const std::size_t points = plot.vectors.front().real.size();
  out += "Values:\n";
  for (std::size_t p = 0; p < points; ++p) {
    out += " " + std::to_string(p);
    for (const Vector& v : plot.vectors) {
      out += "\t" + format_e15(v.real[p]);
      if (complex) {
        out += "," + format_e15(imag_at(v, p));
      }
      out += "\n";
    }
    out += "\n";
  }
}

diag::Status check_plot(const Plot& plot) {
  if (plot.vectors.empty()) {
    return diag::make("SIM-RESULT-004", "plot " + plot.name + " has no vectors");
  }
  const std::size_t points = plot.vectors.front().real.size();
  for (const Vector& v : plot.vectors) {
    if (v.real.size() != points || (v.complex && v.imag.size() != points)) {
      return diag::make("SIM-RESULT-004", "plot " + plot.name + ": vector " + v.name + " has " +
                                              std::to_string(v.real.size()) + " points, the scale " +
                                              std::to_string(points));
    }
  }
  return diag::Done{};
}

}  // namespace

std::string display_name(const Vector& vector) {
  constexpr std::string_view kBranch = "#branch";
  if (vector.name.size() > kBranch.size() && vector.name.ends_with(kBranch)) {
    return "i(" + vector.name.substr(0, vector.name.size() - kBranch.size()) + ")";
  }
  if (vector.type == "voltage" && vector.name.find('(') == std::string::npos) {
    return "v(" + vector.name + ")";
  }
  return vector.name;
}

diag::Outcome<std::string> render_raw(const std::vector<Plot>& plots, const RawWriteOptions& options) {
  std::string out;
  for (const Plot& plot : plots) {
    if (diag::Status ok = check_plot(plot); !ok.ok()) {
      return ok.error();
    }
    const bool complex = any_complex(plot);
    header(out, plot, options, complex);
    if (options.binary) {
      binary_data(out, plot, complex);
    } else {
      ascii_data(out, plot, complex);
    }
  }
  return out;
}

}  // namespace hefesto::sim
