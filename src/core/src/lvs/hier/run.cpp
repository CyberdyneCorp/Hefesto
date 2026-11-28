// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// run_lvs: the hierarchical flow of the `lvs` command (lvs-hierarchy
// "Compare queue construction", "lvs per-pair flow", "Final result",
// "Hierarchy depth limit"; lvs-reports "Subcircuit summary section",
// "Result classification"). Pairs are compared one after the other on the
// calling (worker) thread; each comparison holds its own state (Q-LVS-34).
//
// Owner: LVSHIER.

#include "hefesto/lvs/hier/run.hpp"

#include <optional>

#include "hefesto/lvs/hier/codes.hpp"
#include "hefesto/lvs/hier/hierarchy.hpp"
#include "hefesto/lvs/net/names.hpp"
#include "hefesto/lvs/reduce/reduce.hpp"
#include "hefesto/lvs/report/format.hpp"
#include "lvs/hier/fragments.hpp"
#include "lvs/hier/run_state.hpp"

namespace hefesto::lvs {

PairStatus status_of_unique(int unique) noexcept {
  switch (unique) {
    case 1:
      return PairStatus::match;
    case -3:
      return PairStatus::match_property_errors;
    case -2:
      return PairStatus::match_port_errors;
    case -4:
      return PairStatus::match_port_property_errors;
    case 0:
      return PairStatus::symmetric;
    case -1:
      return PairStatus::black_box;
    default:
      return PairStatus::mismatch;
  }
}

namespace hier_detail {
namespace {

using Classes = std::vector<std::pair<std::string, std::string>>;

std::string cell_label(const Cell& cell) {
  return cell.name + "(" + std::to_string(cell.file) + ")";
}

/// Distinct nets that reach a device pin.
std::uint64_t connected_nets(const Cell& cell) {
  std::vector<bool> seen(cell.nets.size() + 1, false);
  std::uint64_t n = 0;
  for (const auto& instance : cell.instances) {
    for (const NetId net : instance.connections) {
      if (net != kNoNet && net < seen.size() && !seen[net]) {
        seen[net] = true;
        ++n;
      }
    }
  }
  return n;
}

/// The console-only `Contents of circuit N:` block.
std::string contents(const NetlistDb& db, CellId id, int circuit) {
  const Cell& cell = db.cells[id];
  std::string text = "Contents of circuit " + std::to_string(circuit) + ":  Circuit: '" + cell.name + "'\n";
  text += "Circuit " + cell.name + " contains " + std::to_string(cell.instances.size()) + " device instances.\n";
  for (const auto& entry : class_counts(db, id)) {
    text += "  Class: " + entry.name + " instances: " + std::to_string(entry.count) + "\n";
  }
  return text + "Circuit contains " + std::to_string(connected_nets(cell)) + " nets.\n";
}

class PairRun {
 public:
  PairRun(RunState& state, CellId cell1, CellId cell2, bool top)
      : state_(state), c1_(cell1), c2_(cell2), top_(top), cells_{&state.db1, cell1, &state.db2, cell2} {}

  /// The whole per-pair flow; false when the run was cancelled.
  bool run(std::optional<Comparison>& top_comparison, int& top_unique);

 private:
  [[nodiscard]] const Cell& first() const { return state_.db1.cells[c1_]; }
  [[nodiscard]] const Cell& second() const { return state_.db2.cells[c2_]; }

  bool reduce_pair();
  void summary();
  void compare(Comparison& comparison);
  void mismatch_reports(Comparison& comparison);
  bool mismatched() const { return !equivalent_ || unique_ == 0 || unique_ == -2 || unique_ == -4; }
  void decide_flattening();
  void pins(const Comparison& comparison);
  void flatten_pair();
  void fill_references();
  void add_reference(int circuit, ObjectReference::Kind kind, const std::string& name);
  template <typename Fragment>
  void add_fragment_references(const std::vector<MismatchClass<Fragment>>& classes, ObjectReference::Kind kind);

  RunState& state_;
  CellId c1_;
  CellId c2_;
  bool top_;
  PairCells cells_;
  PairReport report_;
  PairResult pair_;
  bool equivalent_ = true;
  int unique_ = -1;
  bool to_flatten_ = false;
};

bool PairRun::reduce_pair() {
  const ReduceOptions options{state_.options.netgen_compat, state_.options.parallel_open, state_.options.cancel};
  state_.text(contents(state_.db1, c1_, 1), true);
  state_.text(contents(state_.db2, c2_, 2), true);
  bool modified = false;
  for (const bool one : {true, false}) {
    const auto summary = reduce(state_.db(one), one ? c1_ : c2_, options);
    for (const auto& line : summary.log) {
      state_.log(line);
    }
    for (const auto& d : summary.diagnostics) {
      state_.result.diagnostics.push_back(d);
    }
    if (summary.cancelled) {
      return false;
    }
    modified = modified || summary.modified();
  }
  if (modified) {
    state_.text("Circuit was modified by parallel/series device merging.\nNew circuit summary:\n" +
                    contents(state_.db1, c1_, 1) + contents(state_.db2, c2_, 2),
                true);
  }
  state_.console("Circuit 1 contains " + std::to_string(first().instances.size()) + " devices, Circuit 2 contains " +
                 std::to_string(second().instances.size()) + " devices.");
  return true;
}

void PairRun::summary() {
  const auto entries1 = class_counts(state_.db1, c1_);
  const auto entries2 = class_counts(state_.db2, c2_);
  const auto identity = state_.identity();
  std::vector<bool> used(entries2.size(), false);
  std::vector<SummaryRow> rows;
  for (const auto& e1 : entries1) {
    std::size_t j = 0;
    while (j < entries2.size() && (used[j] || !identity.same(e1.name, entries2[j].name))) {
      ++j;
    }
    report_.devices1.push_back(DeviceCount{e1.name, e1.count});
    if (j == entries2.size()) {
      rows.push_back(SummaryRow{format_class_count(e1), "(no matching element)"});
      report_.devices2.push_back(DeviceCount{"(no matching element)", 0});
      continue;
    }
    used[j] = true;
    const auto& e2 = entries2[j];
    rows.push_back(SummaryRow{format_class_count(e1),
                              format_class_count(e2) + (e1.count == e2.count ? "" : " **Mismatch**")});
    report_.devices2.push_back(DeviceCount{e2.name, e2.count});
  }
  for (std::size_t j = 0; j < entries2.size(); ++j) {
    if (!used[j]) {
      rows.push_back(SummaryRow{"(no matching element)", format_class_count(entries2[j])});
      report_.devices1.push_back(DeviceCount{"(no matching element)", 0});
      report_.devices2.push_back(DeviceCount{entries2[j].name, entries2[j].count});
    }
  }
  report_.nets1 = connected_nets(first());
  report_.nets2 = connected_nets(second());
  state_.text(summary_section(first().name, second().name, rows, report_, state_.options.widths), false);
}

void PairRun::mismatch_reports(Comparison& comparison) {
  const auto bad = comparison.verify(VerifyOption::none, true);
  const auto matching = comparison.matching();
  report_.bad_nets = net_classes(cells_, bad, matching);
  report_.bad_elements = device_classes(cells_, bad, matching);
}

void PairRun::compare(Comparison& comparison) {
  comparison.converge();
  const auto equivalent = comparison.verify(VerifyOption::equivalent, false);
  equivalent_ = equivalent.value.value_or(-1) != 0;
  report_.converged = comparison.initialized();
  if (equivalent_) {
    pair_ = comparison.resolve();
    unique_ = comparison.verify(VerifyOption::unique, false).value.value_or(-1);
  } else {
    pair_ = comparison.result();
  }
  if (report_.converged) {
    mismatch_reports(comparison);
  }
  for (const auto& line : pair_.log) {
    line.console_only ? state_.console(line.text) : state_.log(line.text);
  }
  const auto& widths = state_.options.widths;
  if (!equivalent_) {
    // The reports of `verify` (no option) for a pair that is not equivalent.
    state_.text(net_report(first().name, second().name, report_.bad_nets, widths), false);
    state_.text(device_report(first().name, second().name, report_.bad_elements, widths), false);
    state_.log("Netlists do not match.");
    return;
  }
  if (unique_ == 0) {
    state_.log("   Networks match locally but not globally.");
    state_.log("   Probably connections are swapped.");
    state_.log("   Check the end of logfile " + state_.options.log_name + " for implicated nodes.");
    if (comparison.verify(VerifyOption::nodes, false).net_report) {
      state_.text(net_report(first().name, second().name, report_.bad_nets, widths), false);
    }
  }
  if (unique_ == -3 || unique_ == -4) {
    state_.result.property_error_cells.push_back(first().name);
    state_.result.property_errors.insert(state_.result.property_errors.end(), pair_.property_errors.begin(),
                                         pair_.property_errors.end());
  }
  report_.properties = property_pairs(cells_, pair_.property_errors);
}

void PairRun::decide_flattening() {
  if (!mismatched() || top_) {
    return;
  }
  auto& p1 = state_.prohibited1;
  auto& p2 = state_.prohibited2;
  if (!p1[c1_] && !p2[c2_]) {
    to_flatten_ = true;
    return;
  }
  state_.log("  Continuing with black-boxed subcircuits " + first().name + " " + second().name);
  state_.result.failed_subcells.push_back(cell_label(first()));
  state_.result.failed_subcells.push_back(cell_label(second()));
  p1[c1_] = true;
  p2[c2_] = true;
  report_.black_boxed = true;
}

void PairRun::pins(const Comparison& comparison) {
  const bool black_box = equivalent_ && unique_ == -1;
  if (!(top_ || (equivalent_ && unique_ != 0) || report_.black_boxed)) {
    return;
  }
  const auto matching = comparison.matching();
  PinMatchInput input;
  input.matching = comparison.initialized() ? &matching : nullptr;
  input.forced = black_box || report_.black_boxed;
  input.top = top_;
  input.netgen_compat = state_.options.netgen_compat;
  input.blackbox = state_.options.blackbox;
  input.case_insensitive = state_.case_insensitive;
  auto result = match_pins(state_.db1, c1_, state_.db2, c2_, input, state_.options.widths);
  for (auto& text : result.output) {
    state_.result.output.push_back(std::move(text));
  }
  if (result.result != 2) {
    report_.pins = std::make_pair(result.pins1, result.pins2);
  }
  if (result.result >= 0) {
    state_.equated.emplace_back(first().name, second().name);
    state_.log("Device classes " + first().name + " and " + second().name + " are equivalent.");
  }
  if (top_) {
    state_.result.top_pin_result = result.result;
    state_.result.top_pin_mismatch = result.result == 0;
  } else if (result.result >= 0) {
    to_flatten_ = false;
  }
}

void PairRun::flatten_pair() {
  if (!to_flatten_) {
    return;
  }
  state_.log("  Flattening non-matched subcircuits " + first().name + " " + second().name);
  for (const bool one : {true, false}) {
    const NetlistDb& db = state_.db(one);
    for (CellId parent = 0; parent < db.cells.size() && !state_.halted(); ++parent) {
      (void)flatten_bounded(state_, one, parent, one ? c1_ : c2_);
    }
  }
  report_.flattened = true;
}

void PairRun::add_reference(int circuit, ObjectReference::Kind kind, const std::string& name) {
  if (auto ref = state_.options.references(circuit, kind, name)) {
    report_.references.push_back(std::move(*ref));
  }
}

template <typename Fragment>
void PairRun::add_fragment_references(const std::vector<MismatchClass<Fragment>>& classes,
                                      ObjectReference::Kind kind) {
  for (const auto& cls : classes) {
    for (const auto& f : cls.first) {
      add_reference(1, kind, f.name);
    }
    for (const auto& f : cls.second) {
      add_reference(2, kind, f.name);
    }
  }
}

void PairRun::fill_references() {
  if (!state_.options.references) {
    return;
  }
  add_fragment_references(report_.bad_nets, ObjectReference::Kind::net);
  add_fragment_references(report_.bad_elements, ObjectReference::Kind::device);
  if (report_.pins) {
    for (const auto& pin : report_.pins->first) {
      add_reference(1, ObjectReference::Kind::pin, pin);
    }
    for (const auto& pin : report_.pins->second) {
      add_reference(2, ObjectReference::Kind::pin, pin);
    }
  }
}

bool PairRun::run(std::optional<Comparison>& top_comparison, int& top_unique) {
  prepare_pair(state_, c1_, c2_, top_);
  if (state_.halted()) {
    return false;
  }
  report_.cell1 = first().name;
  report_.cell2 = second().name;
  report_.file1 = first().file;
  report_.file2 = second().file;
  if (!reduce_pair()) {
    return false;
  }
  summary();
  CompareOptions options;
  options.seed = state_.options.seed;
  options.netgen_compat = state_.options.netgen_compat;
  options.cancel = state_.options.cancel;
  options.strict_topology = state_.options.strict_topology;
  options.equated_classes = state_.equated;
  Comparison comparison{state_.db1, c1_, state_.db2, c2_, options};
  report_.has_devices = comparison.initialized();
  compare(comparison);
  pair_.status = equivalent_ ? status_of_unique(unique_) : PairStatus::mismatch;
  if (pair_.cancelled) {
    state_.result.pairs.push_back(std::move(pair_));
    state_.result.reports.push_back(std::move(report_));
    return false;
  }
  decide_flattening();
  pins(comparison);
  flatten_pair();
  fill_references();
  state_.result.pairs.push_back(std::move(pair_));
  state_.result.reports.push_back(std::move(report_));
  if (top_) {
    top_unique = equivalent_ ? unique_ : 0;
    top_comparison.emplace(std::move(comparison));
  }
  return true;
}

/// `verify only` on the top pair after `Final result: `. A console-only
/// first line (the symmetry verdict with netgen-compat ON, Q-LVS-20) leaves
/// the log ending with `Final result: `.
void write_verdict(RunState& state, Comparison& top, const std::string& title) {
  const auto verify = top.verify(VerifyOption::only, false);
  if (verify.lines.empty() || verify.lines.front().console_only) {
    state.text(title, false);
  }
  for (std::size_t i = 0; i < verify.lines.size(); ++i) {
    const auto& line = verify.lines[i];
    const bool joined = i == 0 && !line.console_only;
    state.text((joined ? title : std::string{}) + line.text + "\n", line.console_only);
  }
}

/// "Final result" and the classification of "Result classification".
void final_result(RunState& state, Comparison& top, int top_unique) {
  LvsResult& result = state.result;
  result.final_output = result.output.size();
  const std::string title = "\nFinal result: ";
  if (result.top_pin_mismatch || !result.failed_subcells.empty()) {
    std::string text = title;
    text += result.failed_subcells.empty() ? "" : "Subcell(s) failed matching.\n";
    text += result.top_pin_mismatch ? "Top level cell failed pin matching.\n" : "";
    state.text(text, false);
  } else {
    write_verdict(state, top, title);
  }
  const auto list = [&state](const std::string& heading, const std::vector<std::string>& names) {
    std::string body = "\n" + heading + "\n ";
    for (std::size_t i = 0; i < names.size(); ++i) {
      body += (i == 0 ? "" : "\n ") + names[i];
    }
    state.text(body + "\n", false);
  };
  if (!result.property_error_cells.empty()) {
    list("The following cells had property errors:", result.property_error_cells);
  }
  if (!result.failed_subcells.empty()) {
    list("The following subcells failed to match:", result.failed_subcells);
  }
  state.console("LVS Done.");
  result.clean = !result.top_pin_mismatch && result.failed_subcells.empty() &&
                 result.property_error_cells.empty() && top_unique == 1;
}

/// The cells of `db` that `options` prohibits from flattening.
std::vector<bool> prohibited_cells(const NetlistDb& db, const RunOptions& options,
                                   const std::vector<std::string>& names) {
  std::vector<bool> prohibited(db.cells.size(), false);
  for (std::size_t id = 0; id < db.cells.size(); ++id) {
    const Cell& cell = db.cells[id];
    for (const auto& pattern : options.noflatten) {
      prohibited[id] = prohibited[id] || pattern_matches(pattern, cell.name);
    }
    for (const auto& name : names) {
      prohibited[id] = prohibited[id] || names_equal(name, cell.name, cell.flags.case_insensitive);
    }
  }
  return prohibited;
}

/// The cell to compare of `circuit`, or LVS-HIER-002.
diag::Outcome<CellId> top_cell(const Circuit& circuit, int index) {
  if (circuit.top && *circuit.top < circuit.db.cells.size()) {
    return *circuit.top;
  }
  for (const auto& file : circuit.db.files) {
    if (circuit.cell.empty() || names_equal(circuit.db.cells[file.top].name, circuit.cell, true)) {
      return file.top;
    }
  }
  return diag::make(hier_codes::kNoTop, "Circuit " + std::to_string(index) + " has no cell to compare" +
                                            (circuit.cell.empty() ? std::string{"."} : ": " + circuit.cell + "."));
}

diag::Status validate(const RunOptions& options) {
  if (options.hierarchy_depth == 0 || options.hierarchy_depth > kMaxHierarchyDepth) {
    return diag::make(hier_codes::kBadDepthSetting, "The hierarchy depth limit must be between 1 and " +
                                                        std::to_string(kMaxHierarchyDepth) + " (got " +
                                                        std::to_string(options.hierarchy_depth) + ").");
  }
  return validate_widths(options.widths);
}

LvsResult input_error(LvsResult result, diag::Diagnostic error) {
  result.input_error = true;
  result.diagnostics.push_back(std::move(error));
  return result;
}

/// Compares the queue's pairs in order, then writes the final result;
/// false when the run was cancelled.
bool run_pairs(RunState& state, const std::vector<std::pair<CellId, CellId>>& queue, ProgressSink& progress) {
  std::optional<Comparison> top;
  int top_unique = 0;
  for (std::size_t k = 0; k < queue.size(); ++k) {
    const auto [c1, c2] = queue[k];
    progress.pair_started(state.db1.cells[c1].name, state.db2.cells[c2].name, k, queue.size());
    if (state.halted() || !PairRun{state, c1, c2, k + 1 == queue.size()}.run(top, top_unique)) {
      return false;
    }
  }
  final_result(state, *top, top_unique);
  return true;
}

/// The two top cells and their hierarchy depths, or the input error.
struct Tops {
  CellId top1 = 0;
  CellId top2 = 0;
  std::size_t depth1 = 0;
  std::size_t depth2 = 0;
};

diag::Outcome<Tops> find_tops(const Circuit& first, const Circuit& second, const RunOptions& options) {
  if (auto status = validate(options); !status) {
    return status.error();
  }
  const auto top1 = top_cell(first, 1);
  const auto top2 = top_cell(second, 2);
  if (!top1 || !top2) {
    return !top1 ? top1.error() : top2.error();
  }
  const auto depth1 = hierarchy_depth(first.db, top1.value(), options.hierarchy_depth);
  const auto depth2 = hierarchy_depth(second.db, top2.value(), options.hierarchy_depth);
  if (!depth1 || !depth2) {
    return !depth1 ? depth1.error() : depth2.error();
  }
  return Tops{top1.value(), top2.value(), depth1.value(), depth2.value()};
}

}  // namespace
}  // namespace hier_detail

LvsResult run_lvs(Circuit first, Circuit second, const SetupState& /*setup*/, const RunOptions& options,
                  ProgressSink& progress) {
  // The setup file's state reaches the run through `options` (lvs/cmd maps
  // SetupState onto RunOptions) and through the databases it wrote into.
  LvsResult result;
  result.netgen_compat = options.netgen_compat;
  const auto tops = hier_detail::find_tops(first, second, options);
  if (!tops) {
    return hier_detail::input_error(std::move(result), tops.error());
  }
  const auto& t = tops.value();
  hier_detail::RunState state{options, first.db, second.db, result};
  state.case_insensitive = compare_case_insensitive(first.db.cells[t.top1], second.db.cells[t.top2]);
  state.equated = options.equated_classes;
  state.prohibited1 = hier_detail::prohibited_cells(first.db, options, options.prohibited1);
  state.prohibited2 = hier_detail::prohibited_cells(second.db, options, options.prohibited2);
  const auto queue = build_compare_queue(
      QueueInput{&first.db, t.top1, t.depth1, &second.db, t.top2, t.depth2, state.identity()});
  for (const auto& [c1, c2] : queue) {
    result.queue.emplace_back(first.db.cells[c1].name, second.db.cells[c2].name);
  }
  if (!hier_detail::run_pairs(state, queue, progress)) {
    if (state.stop) {
      return hier_detail::input_error(std::move(result), *state.stop);
    }
    result.cancelled = true;
    result.diagnostics.push_back(diag::make(hier_codes::kCancelled, "LVS run cancelled."));
  }
  return result;
}

}  // namespace hefesto::lvs
