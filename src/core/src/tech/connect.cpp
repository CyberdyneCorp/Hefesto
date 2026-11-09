// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `connect` section and the connectivity tables (capability
// tech-compose-and-connect, "connect section", "Connectivity finalization").
//
// Ported from Magic VLSI 8.3.694 database/DBtech.c (DBTechInitConnect,
// DBTechAddConnect, DBTechFinalConnect), Copyright (C) 1985, 1990 Regents
// of the University of California (see THIRD_PARTY_NOTICES.md).

#include "tech/build_state.hpp"

namespace hefesto::tech {

namespace {

using Data = Technology::Data;

TileType tt(std::size_t t) noexcept {
  return static_cast<TileType>(t);
}

// Step 1: stacked types connect to their components, the components'
// connections and the stacked types sharing a component.
void connect_stacked(Data& d) {
  for (std::size_t g = d.num_user; g < d.num_types; ++g) {
    d.connect[g] |= d.residues[g];
    for (std::size_t c = kFirstUserType; c < d.num_user; ++c) {
      if (d.residues[g].test(tt(c))) {
        d.connect[g] |= d.connect[c];
      }
    }
    for (std::size_t h = g + 1; h < d.num_types; ++h) {
      if (d.residues[h].intersects(d.residues[g])) {
        d.connect[g].set(tt(h));
      }
    }
  }
}

void make_symmetric(Data& d) {
  for (std::size_t a = kFirstUserType; a < d.num_types; ++a) {
    for (std::size_t b = kFirstUserType; b < d.num_types; ++b) {
      if (d.connect[a].test(tt(b))) {
        d.connect[b].set(tt(a));
      }
    }
  }
}

void build_not_connected(Data& d) {
  for (std::size_t t = 0; t < kMaxTileTypes; ++t) {
    d.not_connect[t] = ~d.connect[t];
  }
  for (const TileType c : d.contact_list) {
    TypeMask joined = d.connect[c];
    for (const TileType other : d.contact_list) {
      if (d.residues[c].intersects(d.residues[other])) {
        joined.set(other);
      }
    }
    for (std::size_t g = d.num_user; g < d.num_types; ++g) {
      if (d.residues[g].test(c)) {
        joined.set(tt(g));
      }
    }
    d.not_connect[c] = ~joined;
  }
}

void build_connection_planes(Data& d) {
  for (const TileType c : d.contact_list) {
    d.conn_planes[c] = d.image_planes[c];
  }
  for (std::size_t t = kFirstUserType; t < d.num_types; ++t) {
    TypeMask list = d.connect[t];
    for (const TileType c : d.contact_list) {
      list.clear_type(c);
    }
    PlaneMask planes = types_to_planes(d, list);
    if (d.type_plane[t] >= 0) {
      planes &= ~plane_bit(static_cast<PlaneId>(d.type_plane[t]));
    }
    d.all_conn_planes[t] = planes & ~d.conn_planes[t];
  }
}

}  // namespace

void connect_init(BuildState& s) {
  Data& d = s.d;
  for (std::size_t t = 0; t < kMaxTileTypes; ++t) {
    d.connect[t] = TypeMask::of(tt(t));
    d.conn_planes[t] = 0;
    d.all_conn_planes[t] = 0;
  }
}

bool connect_line(BuildState& s, Tokens tokens) {
  Data& d = s.d;
  if (tokens.size() != 2) {
    s.r().error("Line must contain exactly 2 lists of types");
    return false;
  }
  const TypeList a = name_mask(d, tokens[0], s.rep, Noise::warning);
  const TypeList b = name_mask(d, tokens[1], s.rep, Noise::warning);
  for (std::size_t t1 = 0; t1 < d.num_types; ++t1) {
    if (!a.types.test(tt(t1))) {
      continue;
    }
    for (std::size_t t2 = 0; t2 < d.num_types; ++t2) {
      if (b.types.test(tt(t2))) {
        d.connect[t1].set(tt(t2));
        d.connect[t2].set(tt(t1));
      }
    }
  }
  return !a.errors && !b.errors;
}

void connect_final(BuildState& s) {
  Data& d = s.d;
  for (std::size_t t = 0; t < d.num_types; ++t) {
    d.conn_planes[t] = 0;
  }
  connect_stacked(d);
  make_symmetric(d);
  build_not_connected(d);
  build_connection_planes(d);
}

}  // namespace hefesto::tech
