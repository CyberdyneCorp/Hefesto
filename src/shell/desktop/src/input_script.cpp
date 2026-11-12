// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/input_script.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

#include "hefesto/shell/canvas_controller.hpp"
#include "hefesto/shell/input_router.hpp"

namespace hefesto::shell {

namespace {

std::string_view trim(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0) s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0) s.remove_suffix(1);
  return s;
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

std::optional<double> number(std::string_view text) {
  const std::string copy(text);
  char* end = nullptr;
  const double value = std::strtod(copy.c_str(), &end);
  if (copy.empty() || *end != '\0' || !std::isfinite(value)) return std::nullopt;
  return value;
}

/// "X,Y [BUTTON]" into `step`.
bool parse_point(std::string_view args, InputStep& step) {
  const std::size_t space = args.find(' ');
  const std::string_view point = args.substr(0, space);
  const std::string button = space == std::string_view::npos ? "left" : lower(trim(args.substr(space)));
  const std::size_t comma = point.find(',');
  if (comma == std::string_view::npos) return false;
  const auto x = number(point.substr(0, comma));
  const auto y = number(point.substr(comma + 1));
  if (!x || !y) return false;
  step.x = static_cast<float>(*x);
  step.y = static_cast<float>(*y);
  if (button == "left") step.button = ScriptButton::left;
  else if (button == "right") step.button = ScriptButton::right;
  else if (button == "middle") step.button = ScriptButton::middle;
  else return false;
  return true;
}

std::optional<std::uint8_t> modifier_bit(std::string_view name) {
  const std::string n = lower(name);
  if (n == "shift") return kModShift;
  if (n == "ctrl" || n == "control") return kModCtrl;
  if (n == "alt" || n == "option") return kModAlt;
  if (n == "super" || n == "cmd" || n == "command") return kModSuper;
  return std::nullopt;
}

/// "[MOD+]...NAME" into `step`.
bool parse_key(std::string_view args, InputStep& step) {
  std::size_t plus = args.find('+');
  while (plus != std::string_view::npos && plus + 1 < args.size()) {
    const auto bit = modifier_bit(args.substr(0, plus));
    if (!bit) return false;
    step.modifiers = static_cast<std::uint8_t>(step.modifiers | *bit);
    args.remove_prefix(plus + 1);
    plus = args.find('+');
  }
  const auto code = input_key_code(args);
  if (!code) return false;
  step.key = *code;
  return true;
}

bool parse_args(InputStep& step, std::string_view args) {
  switch (step.kind) {
    case ScriptKind::move:
    case ScriptKind::down:
    case ScriptKind::up:
      return parse_point(args, step);
    case ScriptKind::key:
      return parse_key(args, step);
    case ScriptKind::text:
      step.text = std::string(args);
      return !args.empty();
    case ScriptKind::wait: {
      const auto ms = number(args);
      if (!ms || *ms < 0.0 || *ms > 60000.0) return false;
      step.wait_ms = static_cast<int>(*ms);
      return true;
    }
  }
  return false;
}

std::optional<ScriptKind> step_kind(std::string_view word) {
  if (word == "move") return ScriptKind::move;
  if (word == "down") return ScriptKind::down;
  if (word == "up") return ScriptKind::up;
  if (word == "key") return ScriptKind::key;
  if (word == "text") return ScriptKind::text;
  if (word == "wait") return ScriptKind::wait;
  return std::nullopt;
}

struct NamedKey {
  std::string_view name;
  int code;
};

constexpr std::array kNamedKeys{
    NamedKey{"escape", raven_key::kEscape},     NamedKey{"esc", raven_key::kEscape},
    NamedKey{"return", raven_key::kEnter},      NamedKey{"enter", raven_key::kEnter},
    NamedKey{"tab", raven_key::kTab},           NamedKey{"space", raven_key::kSpace},
    NamedKey{"backspace", raven_key::kBackspace}, NamedKey{"delete", raven_key::kDelete},
    NamedKey{"up", raven_key::kUp},             NamedKey{"down", raven_key::kDown},
    NamedKey{"left", raven_key::kLeft},         NamedKey{"right", raven_key::kRight},
    NamedKey{"home", raven_key::kHome},         NamedKey{"end", raven_key::kEnd},
    NamedKey{"menu", raven_key::kMenu},
};

}  // namespace

std::optional<int> input_key_code(std::string_view name) {
  if (name.size() == 1) {
    const auto c = static_cast<unsigned char>(name.front());
    if (std::isalnum(c) != 0) return std::toupper(c);  // R_KEY_A = 'A', R_KEY_0 = '0'
    return std::nullopt;
  }
  const std::string n = lower(name);
  for (const NamedKey& k : kNamedKeys) {
    if (k.name == n) return k.code;
  }
  if (n.size() >= 2 && n.front() == 'f') {
    const auto f = number(n.substr(1));
    if (f && *f >= 1.0 && *f <= 12.0 && std::floor(*f) == *f) return raven_key::kF1 + static_cast<int>(*f) - 1;
  }
  return std::nullopt;
}

InputScriptParse parse_input_script(std::string_view text) {
  InputScriptParse out;
  while (!text.empty()) {
    const std::size_t semi = text.find(';');
    const std::string_view part = trim(text.substr(0, semi));
    text.remove_prefix(semi == std::string_view::npos ? text.size() : semi + 1);
    if (part.empty()) continue;
    const std::size_t space = part.find(' ');
    const auto kind = step_kind(part.substr(0, space));
    InputStep step;
    if (kind) step.kind = *kind;
    const std::string_view args = space == std::string_view::npos ? std::string_view{} : trim(part.substr(space));
    if (!kind || !parse_args(step, args)) {
      out.steps.clear();
      out.error = "bad input step: " + std::string(part);
      return out;
    }
    out.steps.push_back(std::move(step));
  }
  return out;
}

std::vector<InputStep> InputPlayer::take_due(double now_ms) {
  std::vector<InputStep> due;
  if (resume_at_ && now_ms < *resume_at_) return due;
  resume_at_.reset();
  while (next_ < steps_.size()) {
    const InputStep& step = steps_[next_++];
    if (step.kind == ScriptKind::wait) {
      resume_at_ = now_ms + step.wait_ms;
      break;
    }
    due.push_back(step);
  }
  return due;
}

}  // namespace hefesto::shell
