// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ExtWriter: the one `.ext` record writer (capability extract-file-format
// "Record grammar", "Record order"; openspec/changes/m7-extraction
// design.md D5, D8). One emitter per record, Magic's spacing and number
// formats (C locale), `cap` records in the canonical order of Q-EXTRACT-7
// and `merge` groups in Magic's hash order. Called by EXTCAP, EXTDEV,
// EXTHIER and EXTCMD.
//
// Scaffold state (tasks.md 3.3): every emitter appends nothing and
// finish() reports CORE-NOTIMPL-008 until EXTNODE lands them (tasks.md
// 5.5).
//
// Owner: INTEGRATION (this header); EXTNODE (the implementation).

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::extract {

/// The header records' values ("Header records"; Q-EXTRACT-10 version).
struct ExtHeader {
  std::int64_t timestamp = 0;
  std::string version = "8.3";
  std::string tech;
  std::string style;
  std::int32_t rscale = 1;
  std::int32_t cscale = 1;
  std::int32_t lscale = 1;
  std::vector<std::int64_t> resist_classes;  // milliohms per square, in class order
};

/// One terminal of a `device` record.
struct ExtTerminal {
  std::string node;
  std::int64_t length = 0;
  std::string attributes = "0";
};

/// Magic's `%lg`-style value formatting used by every numeric field.
[[nodiscard]] std::string format_value(double value);

class ExtWriter {
 public:
  void header(const ExtHeader& header);
  void node(std::string_view name, std::int64_t resistance, double cap, db::Point at, std::string_view type,
            std::span<const std::int64_t> area_perimeter);
  void attr(std::string_view node, db::Rect at, std::string_view type, std::string_view text);
  void equiv(std::string_view a, std::string_view b);
  void port(std::string_view name, std::int32_t index, db::Rect at, std::string_view type);
  void substrate(std::string_view name, std::int64_t resistance, double cap);
  void device(std::string_view device_class, std::string_view model, db::Rect at, std::span<const std::string> params,
              std::string_view substrate, std::span<const ExtTerminal> terminals);
  void parameters(std::string_view model, std::span<const std::string> params);
  void cap(std::string_view a, std::string_view b, double value);
  void resist(std::string_view a, std::string_view b, double value);
  void use(std::string_view def, std::string_view id, std::span<const std::int32_t> transform);
  void merge(std::string_view a, std::string_view b, double cap, std::span<const std::int64_t> area_perimeter);
  void killnode(std::string_view name);
  void distance(std::string_view driver, std::string_view receiver, std::int64_t min, std::int64_t max);
  /// Writes the buffered records in "Record order" and returns the text.
  [[nodiscard]] diag::Outcome<std::string> finish();

 private:
  std::vector<std::string> lines_;
};

}  // namespace hefesto::extract
