// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `lvs` arguments, input resolution and the background run (lvs-command
// "lvs command syntax", "Setup file and input resolution", "Execution
// model", "Headless LVS"; Q-LVS-31).
//
// Owner: LVSCMD.

#include "hefesto/lvs/cmd/lvs_command.hpp"

#include <future>
#include <utility>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lvs/cmd/codes.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"

namespace hefesto::lvs {

namespace {

constexpr std::size_t kMaxNoFlattenEntries = 65536;
constexpr std::size_t kMaxNoFlattenEntry = 4096;

diag::Diagnostic usage(std::string text) {
  return diag::make(cmd_codes::kUsage, std::move(text));
}

/// Sets the flag of an exact option word; false when `word` is none.
bool take_option(const std::string& word, LvsArguments& out) {
  if (word == "-list") {
    out.list = true;
  } else if (word == "-json") {
    out.json = true;
  } else if (word == "-blackbox") {
    out.blackbox = true;
  } else if (word.starts_with("-noflatten=")) {
    out.noflatten = word.substr(11);
  } else {
    return false;
  }
  return true;
}

}  // namespace

diag::Outcome<LvsArguments> parse_lvs_arguments(const std::vector<std::string>& args) {
  LvsArguments out;
  std::vector<std::string> positional;
  for (const auto& word : args) {
    if (word == "-layout" && positional.empty() && !out.layout) {
      out.layout = true;
    } else if (!take_option(word, out)) {
      if (word.starts_with('-') && word.size() > 1) {
        return usage("lvs: unknown option \"" + word + "\".");
      }
      positional.push_back(word);
    }
  }
  if (positional.size() < 2 || positional.size() > 4) {
    return usage("Usage: lvs <name1> <name2> [<setup> [<log>]] [-list] [-json] [-blackbox] "
                 "[-noflatten=<file-or-list>]");
  }
  out.name1 = positional[0];
  out.name2 = positional[1];
  if (positional.size() > 2) {
    out.setup = positional[2];
  }
  if (positional.size() > 3) {
    out.log = positional[3];
  }
  return out;
}

diag::Outcome<NetlistName> parse_netlist_name(std::string_view word) {
  auto parts = pcell::split_tcl_list(word);
  if (!parts || parts.value().empty() || parts.value().size() > 2) {
    return usage("lvs: a name is a file or a list {file cell}: \"" + std::string{word} + "\".");
  }
  NetlistName name;
  name.file = parts.value()[0];
  if (parts.value().size() == 2) {
    name.cell = parts.value()[1];
  }
  return name;
}

bool logging_off(std::string_view log) noexcept {
  return log.find("nolog") != std::string_view::npos;
}

std::vector<platform::Path> setup_candidates(std::string_view name, std::string_view active_pdk,
                                             const std::vector<platform::Path>& pdk_roots) {
  std::vector<platform::Path> candidates{platform::Path{std::string{name}}};
  const platform::Path given{std::string{name}};
  if (given.has_parent_path() || active_pdk.empty() || name.empty()) {
    return candidates;
  }
  const std::string pdk{active_pdk};
  for (const auto& root : pdk_roots) {
    const auto folder = root / pdk / "libs.tech" / "netgen";
    candidates.push_back(folder / given);
    if (name == active_pdk) {
      candidates.push_back(folder / (pdk + "_setup.tcl"));
    }
  }
  return candidates;
}

std::optional<platform::Path> resolve_setup(platform::FileSystem& files, std::string_view name,
                                            std::string_view active_pdk,
                                            const std::vector<platform::Path>& pdk_roots) {
  for (const auto& candidate : setup_candidates(name, active_pdk, pdk_roots)) {
    if (files.exists(candidate)) {
      return candidate;
    }
  }
  return std::nullopt;
}

namespace {

diag::Outcome<std::vector<std::string>> noflatten_file(const std::string& text) {
  std::vector<std::string> entries;
  std::size_t start = 0;
  while (start <= text.size()) {
    const auto end = std::min(text.find('\n', start), text.size());
    const auto line = std::string_view{text}.substr(start, end - start);
    start = end + 1;
    auto words = pcell::split_tcl_list(line);
    if (!words) {
      return usage("lvs: bad line in the -noflatten file: " + words.error().text);
    }
    if (words.value().empty() || words.value().front().starts_with('#')) {
      continue;
    }
    entries.insert(entries.end(), words.value().begin(), words.value().end());
  }
  return entries;
}

}  // namespace

diag::Outcome<std::vector<std::string>> parse_noflatten(std::string_view value, platform::FileSystem& files) {
  const platform::Path path{std::string{value}};
  diag::Outcome<std::vector<std::string>> entries = std::vector<std::string>{};
  if (!value.empty() && files.exists(path)) {
    auto text = files.read(path);
    if (!text) {
      return text.error();
    }
    entries = noflatten_file(text.value());
  } else {
    auto words = pcell::split_tcl_list(value);
    if (!words) {
      return usage("lvs: bad -noflatten list: " + words.error().text);
    }
    entries = std::move(words).value();
  }
  if (!entries) {
    return entries;
  }
  const auto& list = entries.value();
  const bool too_long = std::any_of(list.begin(), list.end(),
                                    [](const std::string& e) { return e.size() > kMaxNoFlattenEntry; });
  if (list.size() > kMaxNoFlattenEntries || too_long) {
    return diag::make(cmd_codes::kNoFlattenLimit, "lvs: the -noflatten list is limited to " +
                                                      std::to_string(kMaxNoFlattenEntries) + " entries of at most " +
                                                      std::to_string(kMaxNoFlattenEntry) + " bytes.");
  }
  return entries;
}

void apply_setup(const SetupState& setup, RunOptions& options) {
  options.equated_classes.insert(options.equated_classes.end(), setup.equated_classes.begin(),
                                 setup.equated_classes.end());
  for (const auto& cell : setup.no_flatten) {
    (cell.circuit == 1 ? options.prohibited1 : options.prohibited2).push_back(cell.cell);
  }
  options.blackbox = options.blackbox || setup.auto_blackbox;
  options.strict_topology = setup.strict_topology;
  options.parallel_open = setup.parallel_open;
  options.widths = ColumnWidths{setup.column_left, setup.column_right};
}

std::string progress_text(std::string_view cell1, std::string_view cell2, std::size_t index, std::size_t count) {
  return std::to_string(index + 1) + "/" + std::to_string(count) + " " + std::string{cell1} + " vs " +
         std::string{cell2};
}

int headless_status(const LvsResult& result) noexcept {
  if (result.input_error) {
    return 2;
  }
  return result.clean && !result.cancelled ? 0 : 1;
}

Session::InputStamp stamp_input(platform::FileSystem& files, const platform::Path& path) {
  return Session::InputStamp{path.string(), files.modified_time(path)};
}

namespace {

class CallbackProgress final : public ProgressSink {
 public:
  explicit CallbackProgress(const std::function<void(const std::string&)>& callback) : callback_(callback) {}
  void pair_started(std::string_view cell1, std::string_view cell2, std::size_t index, std::size_t count) override {
    if (callback_) {
      callback_(progress_text(cell1, cell2, index, count));
    }
  }

 private:
  const std::function<void(const std::string&)>& callback_;
};

}  // namespace

diag::Outcome<LvsResult> run_job(platform::TaskExecutor& tasks, Session& session, LvsJob job,
                                 std::vector<Session::InputStamp> stamps) {
  if (session.running) {
    return diag::make(cmd_codes::kBusy, "An LVS run is already in progress for " + *session.running + ".");
  }
  session.running = job.document;
  session.cancel->store(false);
  job.options.cancel.flag = session.cancel.get();
  std::promise<LvsResult> done;
  auto future = done.get_future();
  tasks.submit([&job, &done] {
    // A task must not throw: an exception (memory exhausted) ends the run
    // as an error instead of leaving the caller waiting.
    try {
      CallbackProgress progress{job.on_progress};
      done.set_value(run_lvs(std::move(job.first), std::move(job.second), job.setup, job.options, progress));
    } catch (const std::exception& e) {
      LvsResult failed;
      failed.input_error = true;
      failed.diagnostics.push_back(diag::make(cmd_codes::kNetlist, std::string{"LVS run failed: "} + e.what()));
      done.set_value(std::move(failed));
    }
  });
  LvsResult result = future.get();
  session.running.reset();
  session.last_results[job.document] = result;
  session.inputs[job.document] = std::move(stamps);
  session.changed.erase(job.document);
  session.headless_status = headless_status(result);
  return result;
}

bool cancel_run(Session& session, std::string_view document) {
  if (!session.running || *session.running != document) {
    return false;
  }
  session.cancel->store(true);
  return true;
}

}  // namespace hefesto::lvs
