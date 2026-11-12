// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Tcl console model (see tcl_console.hpp).
// (Magic ref: tcltk/console.tcl:1-135, tcltk/tkshell.tcl:61-202)

#include "hefesto/shell/tcl_console.hpp"

#include <cstdio>
#include <utility>

namespace hefesto::shell {

namespace {

constexpr std::string_view kInterruptedCode = "SCRIPT-TCL-002";
constexpr std::string_view kSandboxPrefix = "SCRIPT-SANDBOX-";

std::string plural(int count, std::string_view noun) {
  return std::to_string(count) + " " + std::string{noun} + (count == 1 ? "" : "s");
}

// The scanner of console_input_status: one pass over the text with Tcl's
// word rules (a brace-quoted word ignores everything but backslashes).
class CompletenessScan {
 public:
  explicit CompletenessScan(std::string_view text) : text_(text) {}

  ConsoleInputStatus run() {
    while (i_ < text_.size()) step();
    ConsoleInputStatus status;
    status.open_braces = braces_;
    status.open_brackets = brackets_;
    status.open_quote = quote_;
    status.continued = continued_;
    status.complete = braces_ == 0 && brackets_ == 0 && !quote_ && !continued_;
    return status;
  }

 private:
  void step() {
    const char c = text_[i_];
    continued_ = false;
    if (c == '\\') {
      escape();
      return;
    }
    if (braces_ > 0) {
      in_braces(c);
    } else if (c == '#' && command_start_ && !quote_) {
      skip_comment();
      return;
    } else {
      outside(c);
    }
    ++i_;
  }

  void escape() {
    continued_ = i_ + 1 >= text_.size() || (text_[i_ + 1] == '\n' && i_ + 2 >= text_.size());
    command_start_ = false;
    i_ += 2;
  }

  void in_braces(char c) {
    if (c == '{') ++braces_;
    if (c == '}') --braces_;
  }

  void skip_comment() {
    while (i_ < text_.size() && text_[i_] != '\n') {
      if (text_[i_] == '\\') ++i_;  // a backslash-newline continues the comment
      ++i_;
    }
    continued_ = i_ > text_.size();
  }

  void outside(char c) {
    if (c == '"' && (quote_ || word_start_)) {
      quote_ = !quote_;
    } else if (c == '{' && word_start_ && !quote_) {
      ++braces_;
    } else if (c == '[') {
      ++brackets_;
    } else if (c == ']' && brackets_ > 0) {
      --brackets_;
    }
    const bool separator = !quote_ && (c == ' ' || c == '\t' || c == '\n' || c == ';');
    command_start_ = !quote_ && brackets_ >= 0 && (c == '\n' || c == ';' || c == '[' ||
                                                   (command_start_ && (c == ' ' || c == '\t')));
    word_start_ = separator || c == '[';
  }

  std::string_view text_;
  std::size_t i_ = 0;
  int braces_ = 0;
  int brackets_ = 0;
  bool quote_ = false;
  bool continued_ = false;
  bool command_start_ = true;
  bool word_start_ = true;
};

}  // namespace

// --- transcript --------------------------------------------------------------------------

std::string_view console_line_prefix(ConsoleLineKind kind) noexcept {
  switch (kind) {
    case ConsoleLineKind::input: return "% ";
    case ConsoleLineKind::continuation: return "> ";
    case ConsoleLineKind::err: return "▲ ";
    case ConsoleLineKind::error: return "■ ";
    case ConsoleLineKind::banner:
    case ConsoleLineKind::out:
    case ConsoleLineKind::result:
    case ConsoleLineKind::info: break;
  }
  return {};
}

std::string console_line_text(const ConsoleLine& line) {
  if (line.kind == ConsoleLineKind::input && line.text.ends_with('>')) return line.text;  // a bare window prompt
  return std::string{console_line_prefix(line.kind)} + line.text;
}

void ConsoleTranscript::add(ConsoleLineKind kind, std::string_view text) {
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t end = std::min(text.find('\n', start), text.size());
    if (end == text.size() && start == end && start > 0) break;  // trailing newline
    lines_.push_back(ConsoleLine{kind, std::string{text.substr(start, end - start)}});
    start = end + 1;
  }
  while (lines_.size() > limit_) {
    lines_.pop_front();
    ++dropped_;
  }
}

void ConsoleTranscript::add_input(std::string_view command) {
  const std::size_t newline = command.find('\n');
  add(ConsoleLineKind::input, command.substr(0, newline));
  if (newline != std::string_view::npos) add(ConsoleLineKind::continuation, command.substr(newline + 1));
}

std::vector<std::size_t> ConsoleTranscript::visible(ConsoleFilter filter) const {
  std::vector<std::size_t> out;
  out.reserve(lines_.size());
  for (std::size_t i = 0; i < lines_.size(); ++i) {
    const ConsoleLineKind kind = lines_[i].kind;
    if (filter == ConsoleFilter::all || kind == ConsoleLineKind::err || kind == ConsoleLineKind::error) {
      out.push_back(i);
    }
  }
  return out;
}

std::string ConsoleTranscript::text() const {
  std::string out;
  for (const ConsoleLine& line : lines_) {
    out += console_line_text(line);
    out += '\n';
  }
  return out;
}

// --- input -------------------------------------------------------------------------------

std::string ConsoleInputStatus::hint() const {
  if (complete) return "⏎ run";
  std::string open;
  const auto add = [&open](const std::string& part) { open += (open.empty() ? "" : ", ") + part; };
  if (open_braces > 0) add(plural(open_braces, "open brace"));
  if (open_brackets > 0) add(plural(open_brackets, "open bracket"));
  if (open_quote) add("open quote");
  if (continued) add("line continues");
  return open + " · ⏎ run when complete · ⇧⏎ new line · ↑ history";
}

ConsoleInputStatus console_input_status(std::string_view text) {
  return CompletenessScan(text).run();
}

std::optional<std::string> ConsoleHistoryCursor::older(std::string_view draft) {
  const std::vector<std::string>& entries = source_();
  if (entries.empty()) return std::nullopt;
  if (!index_) {
    draft_ = std::string{draft};
    index_ = entries.size();
  }
  if (*index_ == 0) return std::nullopt;
  index_ = std::min(*index_, entries.size()) - 1;
  return entries[*index_];
}

std::optional<std::string> ConsoleHistoryCursor::newer() {
  if (!index_) return std::nullopt;
  const std::vector<std::string>& entries = source_();
  if (*index_ + 1 >= entries.size()) {
    index_.reset();
    return draft_;
  }
  ++*index_;
  return entries[*index_];
}

// --- running ------------------------------------------------------------------------------

double ConsoleRun::seconds(ConsoleClock::time_point now) const {
  return std::chrono::duration<double>(now - start_).count();
}

std::string ConsoleRun::header_text(ConsoleClock::time_point now) const {
  char text[48];
  std::snprintf(text, sizeof text, "● running %.1f s", seconds(now));
  return text;
}

bool ConsoleRun::request_interrupt() noexcept {
  if (interrupted_) return false;
  interrupted_ = true;
  return true;
}

std::vector<std::string> ConsoleRun::take_queued() {
  std::vector<std::string> out = std::move(queued_);
  queued_.clear();
  return out;
}

std::string interrupt_report(std::string_view tcl_message, std::size_t edits, std::string_view where) {
  // The interpreter wraps Tcl's own message (board note 8: "as Tcl reports it").
  constexpr std::string_view kWrapper = "script evaluation interrupted (";
  if (tcl_message.starts_with(kWrapper) && tcl_message.ends_with(')')) {
    tcl_message = tcl_message.substr(kWrapper.size(), tcl_message.size() - kWrapper.size() - 1);
  }
  std::string text = tcl_message.empty() ? std::string{"eval unwound"} : std::string{tcl_message};
  if (!where.empty()) text += " · " + std::string{where};
  text += " · interrupted by the user";
  if (edits > 0) {
    text += " (" + std::to_string(edits) + (edits == 1 ? " edit" : " edits") +
            " kept; undo reverts them as one step)";
  }
  return text;
}

// --- the console --------------------------------------------------------------------------

TclConsole::TclConsole(ConsoleEvaluator& evaluator, ConsoleHistoryCursor::Source history, Clock clock)
    : evaluator_(evaluator), history_(std::move(history)), clock_(std::move(clock)) {}

ConsoleReturn TclConsole::press_return(bool shift) {
  const bool blank = input_.find_first_not_of(" \t\n") == std::string::npos;
  if (!shift && blank) {
    input_.clear();
    touch();
    return ConsoleReturn::empty;
  }
  if (shift || !evaluator_.complete(input_)) {
    input_ += '\n';
    touch();
    return ConsoleReturn::newline;
  }
  std::string command = std::move(input_);
  input_.clear();
  history_.reset();
  if (run_ || evaluator_.busy()) {
    hold(command, true);
    return ConsoleReturn::queued;
  }
  (void)run(command);
  return ConsoleReturn::ran;
}

void TclConsole::history_older() {
  if (auto entry = history_.older(input_)) {
    input_ = std::move(*entry);
    touch();
  }
}

void TclConsole::history_newer() {
  if (auto entry = history_.newer()) {
    input_ = std::move(*entry);
    touch();
  }
}

std::optional<ConsoleCompletion> TclConsole::complete_input(std::size_t cursor) {
  if (running()) return std::nullopt;
  const TclCompletionWord word = tcl_completion_word(input_, cursor);
  if (word.kind == TclWordKind::none) return std::nullopt;
  ConsoleCompletion completion =
      complete_tcl_word(input_, cursor, word, evaluator_.names(word.kind, tcl_prefix_glob(word.prefix)));
  if (completion.candidates.empty()) return std::nullopt;
  if (completion.candidates.size() > 1 && completion.text == input_) {
    transcript_.add(ConsoleLineKind::info, completion_listing(completion.candidates));
    touch();
  }
  input_ = completion.text;
  return completion;
}

void TclConsole::hold(const std::string& command, bool echo_input) {
  if (echo_input) {
    transcript_.add_input(command);
    transcript_.add(ConsoleLineKind::info, "queued until it ends");
  }
  if (run_) {
    run_->queue(command);
  } else {
    held_.push_back(command);
  }
  touch();
}

std::size_t TclConsole::run_held() {
  if (run_ || evaluator_.busy() || held_.empty()) return 0;
  std::vector<std::string> lines = std::move(held_);
  held_.clear();
  for (const std::string& line : lines) (void)run(line, false);
  return lines.size();
}

std::optional<ConsoleEvaluation> TclConsole::run(const std::string& command, bool echo_input) {
  // The command bar while a command runs, or a line typed in a frame that
  // another evaluation pumps: queued until it ends.
  if (run_ || evaluator_.busy()) {
    hold(command, echo_input);
    return std::nullopt;
  }
  (void)run_held();  // lines held earlier run first, in order
  if (echo_input) transcript_.add_input(command);
  run_.emplace(command, clock_(), evaluator_.undo_steps());
  touch();
  const ConsoleEvaluation evaluation = evaluator_.evaluate(directed(command));
  const std::size_t before = run_->undo_steps_at_start();
  const std::size_t after = evaluator_.undo_steps();
  std::vector<std::string> queued = run_->take_queued();
  const bool by_user = run_->interrupt_requested();
  run_.reset();
  ConsoleEvaluation shown = evaluation;
  shown.interrupted = evaluation.interrupted || (by_user && !evaluation.result.ok());
  show_result(shown, evaluation.edits.value_or(after > before ? after - before : 0));
  touch();
  for (const std::string& line : queued) (void)run(line, false);
  return shown;
}

bool TclConsole::interrupt() {
  if (!run_ || !run_->request_interrupt()) return false;
  evaluator_.interrupt();
  touch();
  return true;
}

std::string TclConsole::prompt() const {
  return target_.empty() ? std::string{"%"} : target_ + ">";
}

std::string TclConsole::directed(std::string_view line) const {
  return target_.empty() ? std::string{line} : target_ + " " + std::string{line};
}

void TclConsole::show_result(const ConsoleEvaluation& evaluation, std::size_t edits) {
  const cmd::CommandResult& result = evaluation.result;
  if (result.ok()) {
    const std::string value = result.value.to_string();
    if (!value.empty()) transcript_.add(ConsoleLineKind::result, value);
    return;
  }
  // A sandbox denial names its own code (SCRIPT-SANDBOX-*) after Tcl's message.
  std::string_view code;
  for (const diag::Diagnostic& d : result.diagnostics) {
    if (d.code.starts_with(kSandboxPrefix)) code = d.code;
  }
  for (const diag::Diagnostic& d : result.diagnostics) {
    if (d.severity != diag::Severity::error) continue;
    if (evaluation.interrupted || d.code == kInterruptedCode) {
      transcript_.add(ConsoleLineKind::error, interrupt_report(d.text, edits, evaluation.where));
    } else {
      transcript_.add(ConsoleLineKind::error, d.text + " · " + std::string{code.empty() ? d.code : code});
    }
    return;
  }
  transcript_.add(ConsoleLineKind::error, "error");
}

}  // namespace hefesto::shell
