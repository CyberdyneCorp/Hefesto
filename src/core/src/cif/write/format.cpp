// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The text of each CIF output line (see format.hpp). Ports the formats of
// Magic 8.3.694 (fc5daa9e) cif/CIFwrite.c and graphics/grClip.c
// GrClipTriangle, Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
// Owner: CIF.

#include "format.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>

#include "hefesto/gds/stream/dates.hpp"
#include "hefesto/gds/write/emit.hpp"

namespace hefesto::cif::detail {

namespace {

using gds::saturating_add;
using gds::saturating_mul;

std::string num(std::int64_t v) {
  return std::to_string(v);
}

// a * b / r, saturating, C division.
std::int64_t scaled(std::int64_t a, std::int64_t b, std::int64_t r) {
  return saturating_mul(a, b) / r;
}

// days_from_civil (public domain): days since 1970-01-01 of a civil date.
std::int64_t days_from_civil(std::int64_t y, std::int64_t m, std::int64_t d) {
  y -= m <= 2 ? 1 : 0;
  const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
  const std::int64_t yoe = y - era * 400;
  const std::int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const std::int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

}  // namespace

std::string preamble_text(const Preamble& p) {
  std::string out;
  const auto line = [&out](std::string_view key, std::string_view value) {
    out += "( @@";
    out += key;
    out += " : ";
    out += value;
    out += " );\n";
  };
  line("user", p.user);
  line("machine", p.machine);
  line("source", p.source);
  line("tool", p.tool);
  line("compiled", p.compiled);
  line("technology", p.technology);
  line("version", p.version);
  if (p.description) {
    line("techdesc", *p.description);
  }
  line("style", p.style);
  line("date", p.date);
  return out;
}

std::string ctime_text(std::int64_t utc, std::string_view zone) {
  static constexpr std::array<const char*, 7> kDays{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static constexpr std::array<const char*, 12> kMonths{"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const std::optional<gds::LocalTime> t = gds::to_local_time(utc, zone);
  if (!t || t->month < 0 || t->month > 11) {
    return "?";
  }
  const std::int64_t year = t->year + 1900;
  const std::int64_t days = days_from_civil(year, t->month + 1, t->day);
  const auto weekday = static_cast<std::size_t>(((days % 7) + 11) % 7);  // 1970-01-01 was a Thursday
  std::array<char, 64> buffer{};
  (void)std::snprintf(buffer.data(), buffer.size(), "%.3s %.3s%3lld %.2lld:%.2lld:%.2lld %lld", kDays.at(weekday),
                      kMonths.at(static_cast<std::size_t>(t->month)), static_cast<long long>(t->day),
                      static_cast<long long>(t->hour), static_cast<long long>(t->minute),
                      static_cast<long long>(t->second), static_cast<long long>(year));
  return buffer.data();
}

std::string symbol_start(std::int64_t number, std::int64_t reducer, std::int64_t expander) {
  return "DS " + num(number) + " " + num(reducer) + " " + num(saturating_mul(2, expander)) + ";\n";
}

std::optional<std::string> name_line(std::string_view name, std::string_view prefix) {
  if (name.empty()) {
    return std::nullopt;
  }
  if (name == "(UNNAMED)") {
    return std::string{"9 UNNAMED;\n"};
  }
  if (!prefix.empty()) {
    return "9 " + std::string{prefix} + "/" + std::string{name} + ";\n";
  }
  return "9 " + std::string{name} + ";\n";
}

std::string box_line(const db::Rect& r, std::int64_t reducer) {
  const std::int64_t w = std::int64_t{r.x1} - r.x0;
  const std::int64_t h = std::int64_t{r.y1} - r.y0;
  return "    B " + num(scaled(2, w, reducer)) + " " + num(scaled(2, h, reducer)) + " " +
         num((std::int64_t{r.x1} + r.x0) / reducer) + " " + num((std::int64_t{r.y1} + r.y0) / reducer) + ";\n";
}

std::string triangle_line(const db::TileView& tile, std::int64_t reducer) {
  const std::int64_t xb = tile.area.x0;
  const std::int64_t yb = tile.area.y0;
  const std::int64_t xt = tile.area.x1;
  const std::int64_t yt = tile.area.y1;
  // GrClipTriangle: TT_SIDE is the right side, TT_DIRECTION the `\` diagonal.
  const bool direction = tile.diagonal == db::Diagonal::nw_se;
  std::array<std::int64_t, 6> p{};
  if (!tile.right_side) {
    p = direction ? std::array<std::int64_t, 6>{xb, yt, xb, yb, xt, yb} : std::array<std::int64_t, 6>{xt, yt, xb, yt, xb, yb};
  } else {
    p = direction ? std::array<std::int64_t, 6>{xt, yb, xt, yt, xb, yt} : std::array<std::int64_t, 6>{xb, yb, xt, yb, xt, yt};
  }
  std::string out = "    P";
  for (const std::int64_t v : p) {
    out += " " + num(scaled(2, v, reducer));
  }
  return out + ";\n";
}

std::string layer_label_line(std::string_view layer, const db::Rect& r, const OutputNumbers& n) {
  const std::string center =
      num((std::int64_t{r.x0} + r.x1) / n.reducer) + " " + num((std::int64_t{r.y0} + r.y1) / n.reducer);
  if (n.area_labels) {
    const std::string size = num(scaled(2, std::int64_t{r.x1} - r.x0, n.reducer)) + " " +
                             num(scaled(2, std::int64_t{r.y1} - r.y0, n.reducer));
    return "95 " + std::string{layer} + " " + size + " " + center + ";\n";
  }
  return "94 " + std::string{layer} + " " + center + ";\n";
}

std::string cell_label_line(std::string_view text, const db::Rect& r, const std::optional<std::string>& layer,
                            const OutputNumbers& n) {
  const std::int64_t s = n.scale_factor;
  const std::string center = num(scaled(std::int64_t{r.x0} + r.x1, s, n.reducer)) + " " +
                             num(scaled(std::int64_t{r.y0} + r.y1, s, n.reducer));
  const std::string tail = layer ? " " + *layer + ";\n" : std::string{";\n"};
  if (n.area_labels) {
    const std::int64_t s2 = saturating_mul(2, s);
    const std::string size = num(scaled(std::int64_t{r.x1} - r.x0, s2, n.reducer)) + " " +
                             num(scaled(std::int64_t{r.y1} - r.y0, s2, n.reducer));
    return "95 " + std::string{text} + " " + size + " " + center + tail;
  }
  return "94 " + std::string{text} + " " + center + tail;
}

namespace {

// The `91 ID[(...)]` line of one element.
std::string id_line(const db::CellUse& use, std::int64_t topx, std::int64_t topy, std::int64_t realx,
                    std::int64_t realy) {
  std::string out = "91 " + use.id;
  if (topx > 0 && topy > 0) {
    out += "(" + num(realy) + "," + num(realx) + ")";
  } else if (topx > 0) {
    out += "(" + num(realx) + ")";
  } else if (topy > 0) {
    out += "(" + num(realy) + ")";
  }
  return out + ";\n";
}

// The `C` line of element (x, y).
std::string call_line(const db::CellUse& use, std::int64_t number, const OutputNumbers& n, std::int64_t x,
                      std::int64_t y) {
  const db::Transform& t = use.transform;
  std::string out = "C " + num(number);
  if (t.a != t.e || (t.a == 0 && t.b == t.d)) {
    out += " MX R " + num(-std::int64_t{t.a}) + " " + num(-std::int64_t{t.d});
  } else {
    out += " R " + num(t.a) + " " + num(t.d);
  }
  const std::int64_t xstep = saturating_mul(use.array.xsep, x);
  const std::int64_t ystep = saturating_mul(use.array.ysep, y);
  const std::int64_t tx = saturating_add(saturating_add(t.c, saturating_mul(t.a, xstep)), saturating_mul(t.b, ystep));
  const std::int64_t ty = saturating_add(saturating_add(t.f, saturating_mul(t.d, xstep)), saturating_mul(t.e, ystep));
  const std::int64_t s2 = saturating_mul(2, n.scale_factor);
  return out + " T " + num(scaled(tx, s2, n.reducer)) + " " + num(scaled(ty, s2, n.reducer)) + ";\n";
}

}  // namespace

void call_lines(const db::CellUse& use, std::int64_t number, const OutputNumbers& n, bool ids,
                const std::function<bool(const std::string&)>& visit) {
  const db::ArrayInfo& a = use.array;
  const std::int64_t topx = std::llabs(std::int64_t{a.xhi} - a.xlo);
  const std::int64_t topy = std::llabs(std::int64_t{a.yhi} - a.ylo);
  const std::int64_t xstep = a.xhi > a.xlo ? 1 : -1;
  const std::int64_t ystep = a.yhi > a.ylo ? 1 : -1;
  const bool with_id = ids && !use.id.empty();
  for (std::int64_t x = 0; x <= topx; ++x) {
    for (std::int64_t y = 0; y <= topy; ++y) {
      std::string text = with_id ? id_line(use, topx, topy, a.xlo + x * xstep, a.ylo + y * ystep) : std::string{};
      text += call_line(use, number, n, x, y);
      if (!visit(text)) {
        return;
      }
    }
  }
}

LabelText label_text(std::string_view text, bool magic_compat) {
  LabelText out;
  out.text = std::string{text};
  for (char& c : out.text) {
    const bool special = c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ';';
    if (!special) {
      continue;
    }
    out.unsafe = true;
    if (!magic_compat) {
      c = '_';
      out.changed = true;
    }
  }
  return out;
}

}  // namespace hefesto::cif::detail
