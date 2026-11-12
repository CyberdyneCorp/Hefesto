// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Toolkit-free model of the Tcl console (docs/design/mockups/M4-Console.dc.html,
// approved 2026-11-11; capabilities scripting-auxiliary-tools "Console
// integration", ui-gui-managers "Per-window command entry", scripting-api
// "Output, flushing and escaping", hefesto-scripting "One interpreter per
// open project"). The Console tab of the bottom dock evaluates in the
// project's sandboxed interpreter (script::Interpreter::evaluate), the same
// one that ran the PDK scripts and that the command bar sends its lines to.
//
//   - ConsoleTranscript: the lines with their kind (input `%`, continuation
//     `>`, stdout, result, ▲ stderr, ■ error), at most 10,000, the All /
//     stderr filter, and the text Save… writes.
//   - console_input_status: what a multi-line input still lacks ("1 open
//     brace"); Return runs it only when the interpreter's `info complete`
//     says it is complete, Shift-Return always inserts a line.
//   - ConsoleRun: a running evaluation: elapsed time, the Interrupt card
//     after 2 s, input queued meanwhile (Magic's terminal input queue), the
//     interrupt report ("eval unwound · file:line · interrupted by the
//     user (N edits kept; undo reverts them as one step)"). The evaluator
//     keeps the frames coming while it runs (script/progress.hpp: the
//     interpreter's progress handler pumps one every 50 ms, also inside a
//     pure Tcl loop) and merges the edits of an interrupted command into
//     one undo step named after it.
//   - TclConsole: the transcript, input, shared history and run state
//     together; evaluation goes through a ConsoleEvaluator (TclConsoleSession
//     in the app, a fake in tests).
//
// Owner: SHELL (openspec/changes/m4-scripting/design.md D2, task 10.5).

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/shell/tcl_completion.hpp"

namespace hefesto::shell {

// --- transcript --------------------------------------------------------------------------

/// What a transcript line is; shape plus color, never color alone (board note 2).
enum class ConsoleLineKind : std::uint8_t {
  banner,        // "Tcl 8.6.16 · Hefesto sandbox · project …"
  input,         // "% " + the first line of a command (ember prompt)
  continuation,  // "> " + its other lines
  out,           // `puts` text on stdout (plain)
  result,        // the command's result (secondary text)
  err,           // ▲ stderr text (warning color)
  error,         // ■ a Tcl error with its code (danger color)
  info,          // a note of the console itself ("queued until it ends")
};

struct ConsoleLine {
  ConsoleLineKind kind = ConsoleLineKind::out;
  std::string text;

  friend bool operator==(const ConsoleLine&, const ConsoleLine&) = default;
};

/// The glyph that starts a line of `kind` ("% ", "> ", "▲ ", "■ ", "" …).
[[nodiscard]] std::string_view console_line_prefix(ConsoleLineKind kind) noexcept;
/// The line as shown and saved: prefix + text.
[[nodiscard]] std::string console_line_text(const ConsoleLine& line);

enum class ConsoleFilter : std::uint8_t { all, stderr_only };

class ConsoleTranscript {
 public:
  static constexpr std::size_t kDefaultLimit = 10'000;
  explicit ConsoleTranscript(std::size_t limit = kDefaultLimit) : limit_(limit) {}

  /// Appends `text`, one line per `\n`-separated line (a trailing newline
  /// adds no empty line). The oldest lines go beyond the limit.
  void add(ConsoleLineKind kind, std::string_view text);
  /// The input lines of a command: "% first", "> rest" … (prompt `prompt`
  /// instead of "%" for a window's command entry).
  void add_input(std::string_view command);
  void clear() { lines_.clear(); }

  [[nodiscard]] const std::deque<ConsoleLine>& lines() const noexcept { return lines_; }
  /// The indices shown under `filter` (stderr: ▲ and ■ lines only).
  [[nodiscard]] std::vector<std::size_t> visible(ConsoleFilter filter) const;
  /// Every line as shown, newline-terminated (Save…).
  [[nodiscard]] std::string text() const;
  /// Lines dropped at the top since the last clear (the Table keeps its scroll).
  [[nodiscard]] std::size_t dropped() const noexcept { return dropped_; }

 private:
  std::size_t limit_;
  std::deque<ConsoleLine> lines_;
  std::size_t dropped_ = 0;
};

// --- input -------------------------------------------------------------------------------

/// What an input lacks to be a complete Tcl command (board note 5).
struct ConsoleInputStatus {
  bool complete = true;  // by Tcl's word rules (braces, brackets, quotes, backslash-newline)
  int open_braces = 0;
  int open_brackets = 0;
  bool open_quote = false;
  bool continued = false;  // ends with a backslash-newline
  /// The hint line: "⏎ run" when complete, else "1 open brace" and so on,
  /// then "⏎ run when complete · ⇧⏎ new line · ↑ history".
  [[nodiscard]] std::string hint() const;
};

/// Tcl's word rules over `text` (Tcl_CommandComplete's scan: braces nest and
/// ignore everything but backslashes inside them, quotes end at an unescaped
/// `"`, brackets nest outside braces, `#` comments at command start run to the
/// end of the line). The interpreter's `info complete` decides; this gives
/// the hint and the fallback without an interpreter.
[[nodiscard]] ConsoleInputStatus console_input_status(std::string_view text);

/// Walks a shared history (the command bar's), as a shell: older() from the
/// newest, newer() back to the draft.
class ConsoleHistoryCursor {
 public:
  using Source = std::function<const std::vector<std::string>&()>;
  explicit ConsoleHistoryCursor(Source source) : source_(std::move(source)) {}

  /// Up: the previous entry (or nullopt at the oldest); `draft` is kept.
  std::optional<std::string> older(std::string_view draft);
  /// Down: the next entry, or the draft past the newest.
  std::optional<std::string> newer();
  void reset() { index_.reset(); }

 private:
  Source source_;
  std::optional<std::size_t> index_;
  std::string draft_;
};

// --- running ------------------------------------------------------------------------------

using ConsoleClock = std::chrono::steady_clock;

/// A running evaluation (board tiles 6 and 7).
class ConsoleRun {
 public:
  static constexpr std::chrono::milliseconds kCardDelay{2000};

  ConsoleRun(std::string command, ConsoleClock::time_point start, std::size_t undo_steps)
      : command_(std::move(command)), start_(start), undo_steps_(undo_steps) {}

  [[nodiscard]] const std::string& command() const noexcept { return command_; }
  [[nodiscard]] double seconds(ConsoleClock::time_point now) const;
  /// The Interrupt card and the "● running 4.2 s" header show after 2 s.
  [[nodiscard]] bool card_visible(ConsoleClock::time_point now) const { return now - start_ >= kCardDelay; }
  /// "● running 4.2 s".
  [[nodiscard]] std::string header_text(ConsoleClock::time_point now) const;

  /// Interrupt or Ctrl-C: true the first time (the caller then asks the
  /// interpreter to stop, script::Interpreter::request_interrupt).
  bool request_interrupt() noexcept;
  [[nodiscard]] bool interrupt_requested() const noexcept { return interrupted_; }

  /// Input typed while it runs: queued, run after it in order.
  void queue(std::string line) { queued_.push_back(std::move(line)); }
  [[nodiscard]] std::vector<std::string> take_queued();
  [[nodiscard]] std::size_t undo_steps_at_start() const noexcept { return undo_steps_; }

 private:
  std::string command_;
  ConsoleClock::time_point start_;
  std::size_t undo_steps_ = 0;
  bool interrupted_ = false;
  std::vector<std::string> queued_;
};

/// The transcript line of an interrupted evaluation: "eval unwound ·
/// rc.tcl:12 · interrupted by the user (N edits kept; undo reverts them as
/// one step)", without the location when `where` is empty and without the
/// edits part when nothing changed (board tile 8 and note 8). Tcl's message
/// is taken out of the interpreter's "script evaluation interrupted (…)";
/// without one, Tcl's own text for an unwinding cancel, "eval unwound".
[[nodiscard]] std::string interrupt_report(std::string_view tcl_message, std::size_t edits,
                                           std::string_view where = {});

// --- the console --------------------------------------------------------------------------

/// What an evaluation produced, as the console shows it.
struct ConsoleEvaluation {
  cmd::CommandResult result;
  bool interrupted = false;  // stopped by request_interrupt (SCRIPT-TCL-002)
  /// The undo steps the command made, when the evaluator knows them (an
  /// interrupted command's, merged into one step); nullopt: the console
  /// counts the closed steps itself.
  std::optional<std::size_t> edits;
  /// "file:line" (or "line N") of the last command the run reported while
  /// it ran; empty when unknown.
  std::string where;
};

/// Runs a console command in the project's interpreter. The app's
/// implementation is TclConsoleSession; `puts` and engine messages reach the
/// console through ConsoleTranscript while it runs.
class ConsoleEvaluator {
 public:
  virtual ~ConsoleEvaluator() = default;
  /// Tcl's `info complete` of `text` (true without an interpreter).
  [[nodiscard]] virtual bool complete(std::string_view text) = 0;
  virtual ConsoleEvaluation evaluate(std::string_view command) = 0;
  /// Asks the running evaluation to stop.
  virtual void interrupt() noexcept = 0;
  /// The undo log's closed-step count (edits kept by an interrupt).
  [[nodiscard]] virtual std::size_t undo_steps() const = 0;
  /// True while another evaluation runs in the interpreter (a menu
  /// callback, the PDK startup) and the console is reached from a frame it
  /// pumps: a command typed then is held until it ends.
  [[nodiscard]] virtual bool busy() const { return false; }
  /// The names Tab completes (board note 5): the commands and procedures,
  /// or the variables, matching `glob` (tcl_prefix_glob). None without an
  /// interpreter or while an evaluation runs.
  [[nodiscard]] virtual std::vector<std::string> names(TclWordKind kind, std::string_view glob) {
    (void)kind;
    (void)glob;
    return {};
  }

 protected:
  ConsoleEvaluator() = default;
  ConsoleEvaluator(const ConsoleEvaluator&) = default;
  ConsoleEvaluator& operator=(const ConsoleEvaluator&) = default;
};

/// The Return key's outcome.
enum class ConsoleReturn : std::uint8_t { ran, newline, queued, empty };

class TclConsole {
 public:
  using Clock = std::function<ConsoleClock::time_point()>;

  TclConsole(ConsoleEvaluator& evaluator, ConsoleHistoryCursor::Source history,
             Clock clock = [] { return ConsoleClock::now(); });

  [[nodiscard]] ConsoleTranscript& transcript() noexcept { return transcript_; }
  [[nodiscard]] const ConsoleTranscript& transcript() const noexcept { return transcript_; }

  /// The input area's text (set by the widget on every edit).
  void set_input(std::string text) { input_ = std::move(text); }
  [[nodiscard]] const std::string& input() const noexcept { return input_; }
  [[nodiscard]] ConsoleInputStatus input_status() const { return console_input_status(input_); }

  /// Return: runs the input when it is a complete command (or queues it
  /// while a command runs), else inserts a new line. Shift-Return
  /// (`shift`) always inserts a new line.
  ConsoleReturn press_return(bool shift);
  /// ↑ / ↓ in the input's first / last line: the shared history.
  void history_older();
  void history_newer();
  /// Tab (board note 5): completes the command, procedure or variable name
  /// before byte `cursor` of the input from the interpreter's names; the
  /// input becomes the completed text. When several names match and the
  /// input cannot grow, they are listed in the transcript. nullopt: nothing
  /// to complete (an argument, no match, a command running).
  std::optional<ConsoleCompletion> complete_input(std::size_t cursor);

  /// Runs `command` now (the input of Return, a queued line, or the command
  /// bar's line: `echo_input` false when the bar already shows it): input
  /// lines, then the evaluation's output, result or error; then the lines
  /// queued meanwhile, in order. The running state is visible to frames the
  /// evaluator pumps meanwhile. Returns the evaluation, or nullopt when the
  /// command was queued behind a running one.
  std::optional<ConsoleEvaluation> run(const std::string& command, bool echo_input = true);
  [[nodiscard]] bool running() const noexcept { return run_.has_value(); }
  [[nodiscard]] const std::optional<ConsoleRun>& current_run() const noexcept { return run_; }
  /// Interrupt (header button, the card, Ctrl-C): only while running.
  bool interrupt();
  /// Commands held while another evaluation was busy (ConsoleEvaluator::busy).
  [[nodiscard]] std::size_t held() const noexcept { return held_.size(); }
  /// Runs the held commands, in order, once the evaluator is no longer busy
  /// (the shell calls it from its frame; run() calls it first too). Returns
  /// the number run.
  std::size_t run_held();

  /// Window command entry (Options › Window Command Entry): lines go to
  /// `<window> <command>` with prompt `<window>>`; empty = the project console.
  void set_target_window(std::string window) { target_ = std::move(window); }
  [[nodiscard]] const std::string& target_window() const noexcept { return target_; }
  /// The prompt shown before the input ("%" or "<window>>").
  [[nodiscard]] std::string prompt() const;
  /// The command actually evaluated for `line` (directed to the target window).
  [[nodiscard]] std::string directed(std::string_view line) const;

  [[nodiscard]] ConsoleFilter filter() const noexcept { return filter_; }
  void set_filter(ConsoleFilter filter) noexcept { filter_ = filter; }
  /// Bumped on every change, so the widget refreshes only when needed.
  [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }

 private:
  void show_result(const ConsoleEvaluation& evaluation, std::size_t edits);
  void hold(const std::string& command, bool echo_input);
  void touch() noexcept { ++revision_; }

  ConsoleEvaluator& evaluator_;
  ConsoleHistoryCursor history_;
  Clock clock_;
  ConsoleTranscript transcript_;
  std::string input_;
  std::optional<ConsoleRun> run_;
  std::vector<std::string> held_;  // typed while another evaluation was busy
  std::string target_;
  ConsoleFilter filter_ = ConsoleFilter::all;
  std::uint64_t revision_ = 0;
};

}  // namespace hefesto::shell
