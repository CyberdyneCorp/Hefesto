// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The anchored name patterns of `-noflatten` (lvs-hierarchy "Black boxes
// and flattening prohibitions"). A pattern comes from the user's command
// line or a file, so it is matched by a position-set simulation: linear in
// name length times pattern length, no recursion and no backtracking, on
// every input. Supported: literal bytes, `.`, `[...]` and `[^...]` with
// ranges, `\` escapes, the postfix `*`, `+` and `?`, and an optional
// leading `^` and trailing `$` (the match is anchored anyway).
//
// Owner: LVSHIER.

#include <bitset>
#include <string>
#include <vector>

#include "hefesto/lvs/hier/hierarchy.hpp"

namespace hefesto::lvs {

namespace {

struct Atom {
  std::bitset<256> bytes;
  char quantifier = 0;  // 0, '*', '+' or '?'
};

std::size_t parse_class(std::string_view pattern, std::size_t i, Atom& atom) {
  // pattern[i] == '['
  std::size_t j = i + 1;
  const bool negate = j < pattern.size() && pattern[j] == '^';
  j += negate ? 1 : 0;
  const std::size_t first = j;
  while (j < pattern.size() && (pattern[j] != ']' || j == first)) {
    const auto lo = static_cast<unsigned char>(pattern[j]);
    if (j + 2 < pattern.size() && pattern[j + 1] == '-' && pattern[j + 2] != ']') {
      const auto hi = static_cast<unsigned char>(pattern[j + 2]);
      for (unsigned c = lo; c <= hi; ++c) {
        atom.bytes.set(c);
      }
      j += 3;
    } else {
      atom.bytes.set(lo);
      ++j;
    }
  }
  if (j >= pattern.size()) {  // no closing bracket: a literal '['
    atom.bytes.reset();
    atom.bytes.set(static_cast<unsigned char>('['));
    return i + 1;
  }
  if (negate) {
    atom.bytes.flip();
  }
  return j + 1;
}

/// One atom starting at `i`; returns the index after it.
std::size_t parse_atom(std::string_view pattern, std::size_t i, Atom& atom) {
  const char c = pattern[i];
  if (c == '\\' && i + 1 < pattern.size()) {
    atom.bytes.set(static_cast<unsigned char>(pattern[i + 1]));
    return i + 2;
  }
  if (c == '.') {
    atom.bytes.set();
    return i + 1;
  }
  if (c == '[') {
    return parse_class(pattern, i, atom);
  }
  atom.bytes.set(static_cast<unsigned char>(c));
  return i + 1;
}

std::vector<Atom> parse(std::string_view pattern) {
  if (pattern.starts_with('^')) {
    pattern.remove_prefix(1);
  }
  if (pattern.ends_with('$') && !pattern.ends_with("\\$")) {
    pattern.remove_suffix(1);
  }
  std::vector<Atom> atoms;
  std::size_t i = 0;
  while (i < pattern.size()) {
    Atom atom;
    i = parse_atom(pattern, i, atom);
    if (i < pattern.size() && (pattern[i] == '*' || pattern[i] == '+' || pattern[i] == '?')) {
      atom.quantifier = pattern[i++];
    }
    atoms.push_back(atom);
  }
  return atoms;
}

/// Adds the positions reachable by skipping optional atoms.
void close(const std::vector<Atom>& atoms, std::vector<bool>& states) {
  for (std::size_t k = 0; k < atoms.size(); ++k) {
    if (states[k] && (atoms[k].quantifier == '*' || atoms[k].quantifier == '?')) {
      states[k + 1] = true;
    }
  }
}

}  // namespace

bool pattern_matches(std::string_view pattern, std::string_view name) {
  const auto atoms = parse(pattern);
  std::vector<bool> states(atoms.size() + 1, false);
  states[0] = true;
  close(atoms, states);
  for (const char c : name) {
    std::vector<bool> next(atoms.size() + 1, false);
    for (std::size_t k = 0; k < atoms.size(); ++k) {
      if (!states[k] || !atoms[k].bytes.test(static_cast<unsigned char>(c))) {
        continue;
      }
      next[k + 1] = true;
      if (atoms[k].quantifier == '*' || atoms[k].quantifier == '+') {
        next[k] = true;
      }
    }
    close(atoms, next);
    states = std::move(next);
  }
  return states[atoms.size()];
}

}  // namespace hefesto::lvs
