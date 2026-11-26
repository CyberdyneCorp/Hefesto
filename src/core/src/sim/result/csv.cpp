// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// "CSV export". Owner: SIMRES. See raw.hpp.

#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>

#include "hefesto/sim/result/raw.hpp"

namespace hefesto::sim {

namespace {

std::string g17(double value) {
  std::array<char, 64> buffer{};
  const int n = std::snprintf(buffer.data(), buffer.size(), "%.17g", value);
  return std::string(buffer.data(), static_cast<std::size_t>(n > 0 ? n : 0));
}

// RFC 4180 quoting, only when the name holds `,` or `"`.
std::string quoted(const std::string& name) {
  if (name.find_first_of(",\"") == std::string::npos) {
    return name;
  }
  std::string out = "\"";
  for (const char c : name) {
    out += c == '"' ? std::string{"\"\""} : std::string(1, c);
  }
  return out + "\"";
}

// One CSV column: a header and a value per point (nullopt: empty cell).
struct Column {
  std::string header;
  const Vector* vector = nullptr;
  int part = 0;  // 0 real, 1 imag, 2 db, 3 phase
};

double value_of(const Column& c, std::size_t i) {
  const double re = c.vector->real[i];
  const double im = i < c.vector->imag.size() ? c.vector->imag[i] : 0.0;
  switch (c.part) {
    case 1: return im;
    case 2: return 20.0 * std::log10(std::hypot(re, im));
    case 3: {
      const double degrees = std::atan2(im, re) * 180.0 / std::numbers::pi;
      return degrees <= -180.0 ? 180.0 : degrees;  // in (-180, 180]
    }
    default: return re;
  }
}

void add_columns(std::vector<Column>& columns, const Vector& v, bool dbphase) {
  const std::string name = display_name(v);
  if (!v.complex) {
    columns.push_back(Column{quoted(name), &v, 0});
  } else if (dbphase) {
    columns.push_back(Column{quoted("db(" + name + ")"), &v, 2});
    columns.push_back(Column{quoted("ph(" + name + ")"), &v, 3});
  } else {
    columns.push_back(Column{quoted("re(" + name + ")"), &v, 0});
    columns.push_back(Column{quoted("im(" + name + ")"), &v, 1});
  }
}

bool wanted(const Vector& v, const CsvOptions& options) {
  if (options.vectors.empty()) {
    return true;
  }
  const std::string name = display_name(v);
  for (const std::string& w : options.vectors) {
    if (w == v.name || w == name) {
      return true;
    }
  }
  return false;
}

void write_row(std::string& out, const std::vector<Column>& columns, std::size_t i) {
  for (std::size_t c = 0; c < columns.size(); ++c) {
    out += c == 0 ? "" : ",";
    if (i < columns[c].vector->real.size()) {
      out += g17(value_of(columns[c], i));
    }
  }
  out += "\n";
}

}  // namespace

diag::Outcome<std::string> render_csv(const Plot& plot, const CsvOptions& options) {
  if (plot.vectors.empty()) {
    return diag::make("SIM-RESULT-004", "plot " + plot.name + " has no vectors");
  }
  const Vector& scale = plot.vectors.front();
  std::vector<Column> columns{Column{quoted(display_name(scale)), &scale, 0}};
  for (std::size_t i = 1; i < plot.vectors.size(); ++i) {
    if (wanted(plot.vectors[i], options)) {
      add_columns(columns, plot.vectors[i], options.dbphase);
    }
  }
  std::string out;
  for (std::size_t c = 0; c < columns.size(); ++c) {
    out += (c == 0 ? "" : ",") + columns[c].header;
  }
  out += "\n";
  for (std::size_t i = 0; i < scale.real.size(); ++i) {
    write_row(out, columns, i);
  }
  return out;
}

}  // namespace hefesto::sim
