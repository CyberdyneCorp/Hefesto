// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `lvs` command and cross-probing (lvs-command "lvs command syntax",
// "Setup file and input resolution", "Execution model", "Headless LVS",
// "Cross-probing"; lvs-reports "Log and console routing", "JSON file").
// The netgen form compares two netlist files; `lvs -layout` (the layout
// side through ensure_extracted and render_with_map, M7 G3 and MAPS) and a
// `.sch` side (M8 G4) still fail with CORE-NOTIMPL-009, as does
// layout_circuit.
//
// Owner: LVSCMD.

#include "hefesto/lvs/cmd/commands.hpp"

#include <utility>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/lvs/cmd/codes.hpp"
#include "hefesto/lvs/cmd/lvs_command.hpp"
#include "hefesto/lvs/not_implemented.hpp"
#include "hefesto/lvs/read/reader.hpp"
#include "hefesto/lvs/report/report.hpp"
#include "hefesto/lvs/setup/codes.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::lvs {

namespace {

class LvsCommand {
 public:
  explicit LvsCommand(cmd::CommandContext& context) : ctx_(context), files_(context.services().files) {}

  cmd::CommandResult run(const cmd::Invocation& invocation);

 private:
  cmd::CommandResult start(const cmd::Invocation& invocation);
  void say(const std::string& line) { ctx_.emit(diag::make(cmd_codes::kOutput, line)); }
  void say_text(const std::string& text);
  [[nodiscard]] Session& session() { return ctx_.editor() != nullptr ? ctx_.editor()->lvs : local_session_; }
  [[nodiscard]] std::string active_pdk() const;

  diag::Outcome<Circuit> read_circuit(const NetlistName& name, const ReadOptions& options);
  diag::Status read_circuits(const LvsArguments& args, LvsJob& job);
  diag::Status evaluate(const LvsArguments& args, LvsJob& job);
  diag::Status write_outputs(const LvsArguments& args, const LvsResult& result);
  cmd::CommandResult finish(const LvsArguments& args, const LvsResult& result);

  cmd::CommandContext& ctx_;
  platform::FileSystem& files_;
  Session local_session_;
  std::vector<Session::InputStamp> stamps_;
};

cmd::CommandResult fail(diag::Diagnostic error) {
  return cmd::CommandResult::failure(std::move(error));
}

void LvsCommand::say_text(const std::string& text) {
  std::size_t start = 0;
  while (start < text.size()) {
    const auto end = text.find('\n', start);
    say(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
}

std::string LvsCommand::active_pdk() const {
  const auto* editor = ctx_.editor();
  return editor != nullptr && editor->technology ? editor->technology->name() : std::string{};
}

diag::Outcome<Circuit> LvsCommand::read_circuit(const NetlistName& name, const ReadOptions& options) {
  if (name.file.ends_with(".sch")) {
    return not_implemented("lvs: schematic (.sch) netlist source (waits for the M8 exit, G4)");
  }
  auto detected = resolve_source(Source{name.file, std::nullopt}, files_);
  if (!detected) {
    return detected.error();
  }
  say("Reading netlist file " + name.file + " for " + (name.cell.empty() ? name.file : name.cell));
  Circuit circuit;
  circuit.source = detected.value().source;
  circuit.format = detected.value().format;
  circuit.cell = name.cell;
  const auto top = read_netlist(circuit.db, circuit.source, circuit.format, options);
  if (!top) {
    return top.error();
  }
  circuit.top = top.value();
  if (!name.cell.empty()) {
    circuit.top = circuit.db.find_cell(name.cell, circuit.db.cells[top.value()].file);
    if (!circuit.top) {
      return diag::make(cmd_codes::kNoCell, "Cannot find cell " + name.cell + " in file " + name.file);
    }
  }
  stamps_.push_back(stamp_input(files_, circuit.source.name));
  return circuit;
}

diag::Status LvsCommand::read_circuits(const LvsArguments& args, LvsJob& job) {
  const auto name1 = parse_netlist_name(args.name1);
  const auto name2 = parse_netlist_name(args.name2);
  if (!name1 || !name2) {
    return !name1 ? name1.error() : name2.error();
  }
  if (name1.value().file == name2.value().file) {
    return diag::make(cmd_codes::kSameNetlist, "Both cells are in the same netlist:  Cannot compare!");
  }
  ReadOptions options;
  options.netgen_compat = ctx_.project().compat.netgen_compat;
  options.files = &files_;
  options.sink = &ctx_.sink();
  options.auto_blackbox = args.blackbox;
  // Each circuit has its own database; the files keep netgen's numbers
  // (0 and 1), which the report messages print.
  options.file_number = 0;
  auto first = read_circuit(name1.value(), options);
  if (!first) {
    return first.error();
  }
  options.file_number = 1;
  auto second = read_circuit(name2.value(), options);
  if (!second) {
    return second.error();
  }
  job.document = name1.value().file;
  job.first = std::move(first).value();
  job.second = std::move(second).value();
  return diag::Done{};
}

diag::Status LvsCommand::evaluate(const LvsArguments& args, LvsJob& job) {
  // A name that resolves nowhere goes to evaluate_setup as given: it ends the
  // run with LVS-SETUP-002, or continues without a setup when the name
  // contains `nosetup` (lvs-setup-file "Setup file evaluation").
  const auto path = resolve_setup(files_, args.setup, active_pdk(), ctx_.services().pdks.roots());
  const std::string name = path ? path->string() : args.setup;
  SetupContext context;
  context.first = &job.first;
  context.second = &job.second;
  context.files = &files_;
  context.netgen_compat = job.options.netgen_compat;
  context.seed = job.options.seed;
  context.sandbox.read_only_roots = ctx_.services().pdks.roots();
  const platform::Path document{job.document};
  if (document.has_parent_path()) {
    context.sandbox.read_only_roots.push_back(document.parent_path());
  }
  context.cancel.flag = session().cancel.get();
  auto setup = evaluate_setup(Source{name, std::nullopt}, context);
  if (!setup) {
    return setup.error();
  }
  job.setup = std::move(setup).value();
  for (const auto& message : job.setup.log) {
    say(message.text);
  }
  apply_setup(job.setup, job.options);
  if (path) {
    stamps_.push_back(stamp_input(files_, *path));
  }
  return diag::Done{};
}

diag::Status LvsCommand::write_outputs(const LvsArguments& args, const LvsResult& result) {
  if (!logging_off(args.log)) {
    if (auto status = files_.write_atomic(args.log, write_comp_out(result).value()); !status) {
      return diag::make(cmd_codes::kWrite, "Could not open log file " + args.log + ": " + status.error().text);
    }
  }
  if (args.json) {
    say("Generating JSON file result");
    const auto name = json_file_name(args.log);
    if (auto status = files_.write_atomic(name, write_json(result).value()); !status) {
      return diag::make(cmd_codes::kWrite, "Could not write " + name + ": " + status.error().text);
    }
  }
  return diag::Done{};
}

cmd::CommandResult LvsCommand::finish(const LvsArguments& args, const LvsResult& result) {
  say_text(console_text(result, !logging_off(args.log)));
  for (const auto& d : result.diagnostics) {
    if (!result.input_error) {
      ctx_.emit(d);
    }
  }
  if (result.input_error) {
    return fail(result.diagnostics.empty() ? diag::make(cmd_codes::kNetlist, "LVS input error.")
                                           : result.diagnostics.front());
  }
  if (auto status = write_outputs(args, result); !status) {
    return fail(status.error());
  }
  if (result.cancelled) {
    return cmd::CommandResult::success();
  }
  if (args.list) {
    say("Generating list result");
    return cmd::CommandResult::success(to_list(result).value());
  }
  return cmd::CommandResult::success();
}

cmd::CommandResult LvsCommand::run(const cmd::Invocation& invocation) {
  // Each `lvs` starts from a clear state: the status of an earlier run does
  // not outlive a later one, and the cancel flag of a cancelled run does not
  // stop the next setup evaluation (the flag is the session's).
  Session& state = session();
  state.headless_status.reset();
  if (!state.running) {
    state.cancel->store(false);
  }
  auto result = start(invocation);
  if (!result.ok() && !state.headless_status) {
    state.headless_status = 2;  // a usage or input error ("Headless LVS")
  }
  return result;
}

cmd::CommandResult LvsCommand::start(const cmd::Invocation& invocation) {
  auto parsed = parse_lvs_arguments(invocation.args);
  if (!parsed) {
    return cmd::CommandResult::raised(parsed.error());
  }
  const LvsArguments& args = parsed.value();
  if (args.layout) {
    return fail(not_implemented("lvs -layout (layout netlist from extraction; waits for M7 G3 and MAPS)"));
  }
  LvsJob job;
  job.options.netgen_compat = ctx_.project().compat.netgen_compat;
  job.options.blackbox = args.blackbox;
  job.options.log_name = args.log;
  if (args.blackbox) {
    say("Treating empty subcircuits as black-box cells");
  }
  if (args.noflatten) {
    auto entries = parse_noflatten(*args.noflatten, files_);
    if (!entries) {
      return fail(entries.error());
    }
    job.options.noflatten = std::move(entries).value();
  }
  const auto input = [&] {
    auto status = read_circuits(args, job);
    return status ? evaluate(args, job) : status;
  }();
  if (!input) {
    // A cancelled setup evaluation is a cancelled run, not an input error.
    session().headless_status = input.error().code == setup_codes::kCancelled ? 1 : 2;
    return fail(input.error());
  }
  if (!logging_off(args.log)) {
    say("Comparison output logged to file " + args.log);
  }
  auto result = run_job(ctx_.services().tasks, session(), std::move(job), std::move(stamps_));
  if (!result) {
    return fail(result.error());
  }
  return finish(args, result.value());
}

}  // namespace

bool register_lvs_commands(cmd::CommandRegistry& registry) {
  cmd::CommandSpec spec{
      "lvs",
      "lvs <name1> <name2> [<setup> [<log>]] [-list] [-json] [-blackbox] [-noflatten=<file-or-list>]"
      " | lvs -layout <cell> <schematic> [<setup> [<log>]] [option ...]",
      "Compares two netlists (layout versus schematic) as netgen's lvs does (capability lvs-command).",
      [](cmd::CommandContext& context, const cmd::Invocation& invocation) {
        return LvsCommand{context}.run(invocation);
      }};
  return !registry.add(std::move(spec)).has_value();
}

diag::Outcome<Circuit> layout_circuit(editor::EditorState& /*editor*/, std::string_view /*cell*/,
                                      const netlist::spice::SpiceSettings& /*settings*/) {
  return not_implemented("lvs::layout_circuit");
}

namespace {

ObjectReference::Kind reference_kind(ObjectRef::Kind kind) {
  switch (kind) {
    case ObjectRef::Kind::net:
      return ObjectReference::Kind::net;
    case ObjectRef::Kind::device:
      return ObjectReference::Kind::device;
    case ObjectRef::Kind::pin:
      return ObjectReference::Kind::pin;
  }
  return ObjectReference::Kind::net;
}

}  // namespace

diag::Outcome<std::vector<LayoutRef>> cross_probe(const LvsResult& result, const ObjectRef& object) {
  std::vector<LayoutRef> places;
  if (object.pair < result.reports.size()) {
    for (const auto& ref : result.reports[object.pair].references) {
      if (ref.source == ObjectReference::Source::layout && ref.kind == reference_kind(object.kind) &&
          ref.circuit == object.circuit && ref.name == object.name) {
        places.push_back(LayoutRef{ref.use_path, ref.point});
      }
    }
  }
  if (places.empty()) {
    return diag::make(cmd_codes::kNoCounterpart, "no layout counterpart");
  }
  return places;
}

}  // namespace hefesto::lvs
