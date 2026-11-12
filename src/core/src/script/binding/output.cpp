// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Engine output, prompts and the terminal input queue (see output.hpp).
// Ported from Magic 8.3.694 (fc5daa9e) tcltk/tclmagic.c, Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).

#include "hefesto/script/binding/output.hpp"

#include <charconv>
#include <cstdint>
#include <utility>


namespace hefesto::script {

namespace {

std::string_view stream_name(OutputStream stream) {
  return stream == OutputStream::err ? "stderr" : "stdout";
}

std::string quoted_text(std::string_view text) {
  return "\"" + escape_output(text) + "\"";
}

// An integer Tcl result (Tcl_GetIntFromObj on the string form), or nullopt.
std::optional<std::int64_t> integer_of(const cmd::Value& value) {
  if (const auto* number = std::get_if<std::int64_t>(&value.data())) {
    return *number;
  }
  const std::string text = value.to_string();
  std::int64_t parsed = 0;
  const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (ec != std::errc{} || end != text.data() + text.size() || text.empty()) {
    return std::nullopt;
  }
  return parsed;
}

}  // namespace

std::string escape_output(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    if (c == '"' || c == '[' || c == ']' || c == '\\' || c == '$') {
      out += '\\';
    }
    out += c;
  }
  return out;
}

std::string puts_script(OutputStream stream, std::string_view text) {
  return "puts -nonewline " + std::string{stream_name(stream)} + " " + quoted_text(text);
}

cmd::CommandResult write_output(Interpreter& interp, const OutputRouting& routing, OutputStream stream,
                                std::string_view text) {
  if (routing.suspended) {
    return cmd::CommandResult::success();
  }
  cmd::CommandResult printed = interp.evaluate(puts_script(stream, text));
  if (!printed.ok()) {
    return printed;
  }
  // Tcl_stdflush: the original flush, keeping the interpreter's result.
  if (interp.has_command("::tcl_flush")) {
    static_cast<void>(interp.evaluate("::tcl_flush " + std::string{stream_name(stream)}));
  }
  return cmd::CommandResult::success();
}

void ScriptOutputSink::emit(const diag::Diagnostic& diagnostic) {
  const std::optional<std::string> text = diag::render(diagnostic, format_);
  if (!text) {
    return;
  }
  const bool error = diagnostic.severity == diag::Severity::error || diagnostic.severity == diag::Severity::warning;
  const std::string line = diagnostic.no_newline ? *text : *text + "\n";
  static_cast<void>(write_output(interp_, routing_, error ? OutputStream::err : OutputStream::out, line));
}

std::optional<std::string> read_line(Interpreter& interp, const OutputRouting& routing, std::string_view prefix,
                                     std::size_t max_length) {
  cmd::CommandResult answer;
  if (routing.console) {
    answer = interp.evaluate(prefix.empty() ? std::string{"magic::dialog"}
                                            : "magic::dialog \"\" " + quoted_text(prefix));
  } else {
    if (!prefix.empty()) {
      static_cast<void>(write_output(interp, routing, OutputStream::out, prefix));
    }
    answer = interp.evaluate("gets stdin");
  }
  if (!answer.ok()) {
    return std::nullopt;
  }
  std::string line = answer.value.to_string();
  if (!line.empty() && line.back() == '\n') {
    line.pop_back();
  }
  if (max_length > 0 && line.size() > max_length - 1) {
    line.resize(max_length - 1);
  }
  if (line.empty()) {
    return std::nullopt;
  }
  return line;
}

std::size_t ScriptPromptHost::ask(std::string_view question, std::span<const std::string> answers,
                                  std::size_t default_answer) {
  const std::optional<std::size_t> index = try_ask(question, answers, default_answer);
  return index && *index < answers.size() ? *index : default_answer;
}

std::optional<std::size_t> ScriptPromptHost::try_ask(std::string_view question, std::span<const std::string> answers,
                                                     std::size_t default_answer) {
  std::string script = "tk_dialog .dialog \"Dialog\" " + quoted_text(question) + " {} " + std::to_string(default_answer);
  for (const std::string& answer : answers) {
    script += " " + quoted_text(answer);
  }
  const cmd::CommandResult result = interp_.evaluate(script);
  const std::optional<std::int64_t> index = result.ok() ? integer_of(result.value) : std::nullopt;
  if (!index || *index < 0) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(*index);
}

TerminalInputQueue::TerminalInputQueue(Reader terminal, std::string buffered)
    : terminal_(std::move(terminal)), pending_(std::move(buffered)) {}

void TerminalInputQueue::queue(std::string_view bytes) {
  pending_.append(bytes);
}

std::size_t TerminalInputQueue::cancel_partial_line() {
  const std::size_t line_end = pending_.rfind('\n');
  const std::size_t keep = line_end == std::string::npos ? 0 : line_end + 1;
  const std::size_t removed = pending_.size() - keep;
  pending_.resize(keep);
  return removed;
}

std::optional<std::string> TerminalInputQueue::next_line() {
  std::size_t newline = pending_.find('\n');
  while (newline == std::string::npos) {
    std::optional<std::string> more = terminal_ ? terminal_() : std::nullopt;
    if (!more) {
      break;
    }
    pending_ += *more;
    newline = pending_.find('\n');
  }
  if (newline == std::string::npos) {
    if (pending_.empty()) {
      return std::nullopt;
    }
    return std::exchange(pending_, std::string{});
  }
  std::string line = pending_.substr(0, newline);
  pending_.erase(0, newline + 1);
  return line;
}

}  // namespace hefesto::script
