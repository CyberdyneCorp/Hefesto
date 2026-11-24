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
// Records are buffered per record class and finish() writes the classes
// in "Record order"; within a class the records keep the order of the
// calls, except `cap` records, which are written in the canonical order of
// Q-EXTRACT-7 (byte-wise by the smaller terminal name, then the larger;
// the smaller name first in the record). `merge` groups keep the call
// order: EXTHIER calls merge()/merge_more() while it walks its
// util::MagicHashTable, which is Magic's hash order. Numbers use
// format_value() (`%lg`/`%g`, C locale) and `%d` for integers.
//
// Owner: INTEGRATION (this header); EXTNODE (the implementation).

#pragma once

#include <array>
#include <cstdint>
#include <optional>
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
  /// Added in M7 R3 (EXTNODE): the third `scale` field (`%g` of
  /// exts_unitsPerLambda, a float in Magic). When set it is written
  /// instead of `lscale`.
  std::optional<double> units_per_lambda;
  /// Added in M7 R3: the cell's `parameter` property, written as
  /// `parameters :<cell_name> <text>` at the end of the header.
  std::string cell_name;
  std::optional<std::string> cell_parameter;
};

/// The class of a `cap` record ("Record order"): flat coupling, the
/// hierarchical subcell corrections, the array corrections.
enum class CapClass : std::uint8_t { flat, subcell, array };

/// The class of a `merge` group: subcell or array.
enum class MergeClass : std::uint8_t { subcell, array };

/// One terminal of a `device` record.
struct ExtTerminal {
  std::string node;
  std::int64_t length = 0;
  std::string attributes = "0";
};

/// Magic's `%lg`-style value formatting used by every numeric field:
/// C `printf("%g")` in the C locale (6 significant digits, trailing zeros
/// and the point dropped, exponent form `d.ddddde±XX` below 1e-4 and from
/// 1e6 on; `%lg` and `%g` print a double the same way).
[[nodiscard]] std::string format_value(double value);

class ExtWriter {
 public:
  void header(const ExtHeader& header);
  void node(std::string_view name, std::int64_t resistance, double cap, db::Point at, std::string_view type,
            std::span<const std::int64_t> area_perimeter);
  void attr(std::string_view node, db::Rect at, std::string_view type, std::string_view text);
  void equiv(std::string_view a, std::string_view b);
  void port(std::string_view name, std::int32_t index, db::Rect at, std::string_view type);
  /// `substrate "<name>" <R> 0` (the cap is always written 0, as Magic
  /// does); the location form below is what extOutputNodes writes.
  void substrate(std::string_view name, std::int64_t resistance, double cap);
  /// Added in M7 R3: `substrate "<name>" <R> 0 <x> <y> <type> <a0> <p0> ...`.
  void substrate(std::string_view name, std::int64_t resistance, db::Point at, std::string_view type,
                 std::span<const std::int64_t> area_perimeter);
  /// `device <class> <model> <rect> <params...> ["<substrate>"] <terminals>`
  /// (`fet <model> ...` for the class `fet`); `params` are written as
  /// given (the dimensions first), the substrate is omitted when empty, the
  /// first terminal is the gate, each terminal is `"<node>" <length>
  /// <attributes>`.
  void device(std::string_view device_class, std::string_view model, db::Rect at, std::span<const std::string> params,
              std::string_view substrate, std::span<const ExtTerminal> terminals);
  /// Added in M7 R3: a line of the device section written as given (the
  /// `primitive` line, a property device).
  void device_line(std::string_view text);
  /// Added in M7 R3: the `parameters :<cell> <text>` record of a `device
  /// primitive ...` cell property (after the `use` records).
  void cell_parameters(std::string_view cell, std::string_view text);
  /// Added in M7 R3: the `abstract` record (a cell with `LEFview`).
  void abstract();
  void parameters(std::string_view model, std::span<const std::string> params);
  /// A flat `cap` record (CapClass::flat).
  void cap(std::string_view a, std::string_view b, double value);
  /// Added in M7 R3: a `cap` record of the given class.
  void cap(std::string_view a, std::string_view b, double value, CapClass cls);
  void resist(std::string_view a, std::string_view b, double value);
  /// `use <def> <id> a b c d e f`; `id` carries the array suffix
  /// (`A[0:3:10][0:1:20]`) when the use is an array.
  void use(std::string_view def, std::string_view id, std::span<const std::int32_t> transform);
  /// The first `merge` of a group (with its values), MergeClass::subcell.
  void merge(std::string_view a, std::string_view b, double cap, std::span<const std::int64_t> area_perimeter);
  /// Added in M7 R3: the first `merge` of a group of the given class.
  void merge(std::string_view a, std::string_view b, double cap, std::span<const std::int64_t> area_perimeter,
             MergeClass cls);
  /// Added in M7 R3: a value-less continuation `merge "<a>" "<b>"`.
  void merge_more(std::string_view a, std::string_view b, MergeClass cls = MergeClass::subcell);
  void killnode(std::string_view name);
  void distance(std::string_view driver, std::string_view receiver, std::int64_t min, std::int64_t max);
  /// Writes the buffered records in "Record order" and returns the text;
  /// the writer is empty afterwards. EXTRACT-FILE-001 when a `node`,
  /// `substrate` or `merge` record does not hold one area/perimeter pair
  /// per resistance class of the header, EXTRACT-FILE-002 without a
  /// header.
  [[nodiscard]] diag::Outcome<std::string> finish();

  /// Added in M7 R3: the section a record goes to (record classes of
  /// "Record order"; `resist`/`killnode`, written by extresist only, last).
  enum class Section : std::uint8_t {
    header,
    use,
    cell_parameters,
    parameters,
    abstract,
    port,
    node,
    flat_cap,
    device,
    subcell_cap,
    subcell_merge,
    array_cap,
    array_merge,
    distance,
    resist,
    count
  };

 private:
  struct CapRecord {
    std::string a;
    std::string b;
    double value = 0.0;
  };
  struct PairCheck {
    std::size_t pairs = 0;
    std::string record;
  };

  void add(Section section, std::string line);

  std::vector<std::string> lines_;  // unused (scaffold field, kept)
  std::array<std::vector<std::string>, static_cast<std::size_t>(Section::count)> sections_{};
  std::array<std::vector<CapRecord>, 3> caps_{};
  std::vector<PairCheck> pair_checks_;
  std::size_t resist_classes_ = 0;
  bool has_header_ = false;
};

}  // namespace hefesto::extract
