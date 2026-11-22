// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DefReadNets (capability def-read "NETS and SPECIALNETS grammar", "Net
// annotation mode"): each net's properties, its routing statements through
// DefAddRoutes (routes.cpp), and in annotation mode the net label on the
// paint under one of its terminals.
// - Q-LEFDEF-74: a net NONDEFAULTRULE applies to the next routing
//   statement (magic-compat ON) or to the whole net (OFF: the statement is
//   read ahead for its rule, then replayed).
// - Q-LEFDEF-81: the terminal is looked up in the edit cell over its area
//   (ON) or in the design cell over its area grown by 1 (OFF).
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defRead.c:986-1281, Copyright
// (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: DEFR (design D2).

#include <array>
#include <string>
#include <vector>

#include "def/read/reader.hpp"
#include "hefesto/cell/hier.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::def::detail {

namespace {

enum Property : std::size_t {
  kUse,
  kRouted,
  kNoShield,
  kFixed,
  kCover,
  kSource,
  kShieldNet,
  kSubnet,
  kVpin,
  kXtalk,
  kNonDefaultRule,
  kFixedBump,
  kFrequency,
  kOriginal,
  kPattern,
  kEstCap,
  kWeight,
  kProperty,
};

constexpr std::array<std::string_view, 18> kProperties{
    "USE",       "ROUTED",  "NOSHIELD", "FIXED",   "COVER",          "SOURCE",
    "SHIELDNET", "SUBNET",  "VPIN",     "XTALK",   "NONDEFAULTRULE", "FIXEDBUMP",
    "FREQUENCY", "ORIGINAL", "PATTERN", "ESTCAP",  "WEIGHT",         "PROPERTY",
};

constexpr std::array<std::string_view, 2> kStatementKeys{"-", "END"};

bool ends_net(const std::string& token, const Reader& r) {
  return token.starts_with(';') || (token.empty() && r.ended());
}

}  // namespace

class NetReader {
 public:
  NetReader(Reader& reader, bool special) : r_(reader), special_(special) {}

  void run() {
    std::string token = r_.take();
    name_ = token;
    need_annotation_ = r_.context().options.annotate;
    rules_ = nullptr;
    if (!r_.magic_compat()) {
      read_ahead();  // Q-LEFDEF-74 OFF
    }
    while (!ends_net(token, r_)) {
      if (need_annotation_ && token.starts_with('(')) {
        token = connection();
      }
      if (!token.starts_with('+')) {
        token = r_.take();
        continue;
      }
      token = property(r_.take());
    }
  }

 private:
  // The net statement read through its `;` and put back, with the net's
  // rule taken from its last NONDEFAULTRULE.
  void read_ahead() {
    std::vector<std::string> tokens;
    while (auto token = r_.next()) {
      tokens.push_back(std::move(*token));
      if (tokens.back().starts_with(';')) {
        break;
      }
    }
    for (std::size_t i = 0; i + 2 < tokens.size(); ++i) {
      if (tokens[i].starts_with('+') && lef::keyword_is(tokens[i + 1], "NONDEFAULTRULE")) {
        if (const auto* set = r_.context().rules_table.find(tokens[i + 2]); set != nullptr && set->value) {
          rules_ = &*set->value;
        }
      }
    }
    whole_net_rules_ = rules_;
    r_.push_front(std::move(tokens));
  }

  // `( comp pin )` or `( PIN name )` while annotating.
  std::string connection() {
    std::string token = r_.take();
    if (token == "PIN") {
      need_annotation_ = false;
      return token;
    }
    const std::string component = token;
    token = r_.take();
    if (r_.annotate_terminal(component, token, name_)) {
      need_annotation_ = false;
    }
    return token;
  }

  // One `+` property; returns the token the loop goes on with.
  std::string property(const std::string& keyword) {
    const auto key = lef::lookup_full(keyword, kProperties);
    if (!key) {
      r_.message("Unknown net property \"" + keyword + "\" in NET definition; ignoring.");
      return keyword;
    }
    switch (*key) {
      case kRouted:
      case kFixed:
      case kCover:
      case kNoShield:
        return routes();
      case kNonDefaultRule:
        return rule();
      case kProperty:
        (void)r_.take();
        return r_.take();
      case kFixedBump:
        return keyword;
      default:
        return r_.take();  // USE, SOURCE, SHIELDNET, SUBNET, VPIN, XTALK, FREQUENCY, ...
    }
  }

  std::string routes() {
    Reader::RouteRequest request;
    request.special = special_;
    request.rules = rules_;
    const auto& options = r_.context().options;
    if (options.labels && (need_annotation_ || !options.annotate)) {
      request.label = name_;
    }
    std::string token = r_.add_routes(request);
    if (r_.magic_compat()) {
      rules_ = nullptr;  // Q-LEFDEF-74 ON: the next routing statement only
    }
    return token;
  }

  std::string rule() {
    std::string name = r_.take();
    const auto* set = r_.context().rules_table.find(name);
    if (set == nullptr || !set->value) {
      r_.error("Unknown nondefault rule \"" + name + "\"");
    } else if (r_.magic_compat()) {
      rules_ = &*set->value;
    } else {
      rules_ = whole_net_rules_;
    }
    return name;
  }

  Reader& r_;
  bool special_;
  std::string name_;
  bool need_annotation_ = false;
  const lef::NonDefaultRule* rules_ = nullptr;
  const lef::NonDefaultRule* whole_net_rules_ = nullptr;
};

void Reader::nets(std::int32_t total, bool special) {
  (void)design();
  std::int32_t processed = 0;
  const std::string section = special ? "SPECIALNETS" : "NETS";
  while (const auto token = next()) {
    const auto key = lef::lookup_full(*token, kStatementKeys);
    if (!key) {
      message("Unknown keyword \"" + *token + "\" in NET definition; ignoring.");
      skip_statement();
      continue;
    }
    if (*key == 1) {
      if (end_matches(section)) {
        break;
      }
      error("Net END statement missing.");
      continue;
    }
    estimate(total);
    ++processed;
    NetReader{*this, special}.run();
  }
  count_check(processed, total,
              "  Processed " + std::to_string(processed) + (special ? " special" : "") + " nets total.", "nets");
}

// CmdFindNetProc on `component/pin` (unread cells on the path are read),
// then the first tile connecting to its
// type over its area in the design cell (Q-LEFDEF-81).
bool Reader::annotate_terminal(const std::string& component, const std::string& pin, const std::string& net) {
  db::CellDef* lookup_cell = magic_compat() ? context_.edit_cell : design_;
  if (lookup_cell == nullptr) {
    return false;
  }
  const db::Label* found = nullptr;
  db::Transform to_root;
  // DBTreeFindUse reads each cell it meets that is not read yet (Magic
  // prints "Cell <c> read from path ..." here): a terminal of a subcell
  // that `load` left unread is found through its labels.
  cell::CellReader read;
  if (context_.options.read_master) {
    read = [this](db::CellDef& def) { (void)read_master(def); };
  }
  const auto instance = cell::find_instance(*lookup_cell, component, read);
  const db::CellDef* where = instance ? instance->use->def : lookup_cell;
  const std::string leaf = instance ? pin : component + "/" + pin;
  if (instance) {
    to_root = instance->to_root;
  }
  for (const db::Label& l : where->labels()) {
    if (l.text == leaf) {
      found = &l;
      break;
    }
  }
  const auto plane = found != nullptr ? context_.tech.home_plane(found->type) : std::nullopt;
  if (!plane || *plane >= design().plane_count()) {
    return false;
  }
  db::Rect area = db::apply(to_root, found->rect);
  if (!magic_compat()) {
    area = db::Rect{area.x0 - 1, area.y0 - 1, area.x1 + 1, area.y1 + 1};
  }
  std::optional<db::Rect> tile;
  design().plane(*plane).enumerate(area, context_.tech.connects_to(found->type), [&](const db::TileView& t) {
    tile = t.area;
    return false;
  });
  if (!tile) {
    return false;
  }
  db::Label l;
  l.text = net;
  l.type = found->type;
  const db::Coord cx = static_cast<db::Coord>((std::int64_t{tile->x0} + tile->x1) / 2);
  const db::Coord cy = static_cast<db::Coord>((std::int64_t{tile->y0} + tile->y1) / 2);
  l.rect = db::Rect{cx, cy, cx, cy};
  l.justification = db::kCenter;
  (void)db::put_label(design(), std::move(l));
  return true;
}

}  // namespace hefesto::def::detail
