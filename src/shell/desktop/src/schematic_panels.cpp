// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The schematic editor's panels (schematic_panels.hpp). Owner: SHELL.

#include "hefesto/shell/schematic_panels.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <system_error>
#include <utility>

#include "hefesto/diag/format.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/shell/manager_views.hpp"

namespace hefesto::shell {

namespace {

std::string schematic(std::vector<std::string> words) {
  words.insert(words.begin(), "schematic");
  return command_line(words);
}

std::string plural(std::size_t n, std::string_view noun) {
  return std::to_string(n) + " " + std::string{noun} + (n == 1 ? "" : "s");
}

// A diagnostic code: letters, digits and dashes with at least one dash
// ("SCHEM-NET-004", "CORE-NOTIMPL-007").
bool is_code(std::string_view word) {
  if (word.empty() || word.find('-') == std::string_view::npos) return false;
  return std::all_of(word.begin(), word.end(), [](char c) {
    return std::isupper(static_cast<unsigned char>(c)) != 0 || std::isdigit(static_cast<unsigned char>(c)) != 0 ||
           c == '-';
  });
}

std::size_t line_count(std::string_view text) {
  if (text.empty()) return 0;
  const auto lf = static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
  return text.back() == '\n' ? lf : lf + 1;
}

struct HighlightEntry {
  std::string name;
  bool instance = false;
  int colour = 0;
};

// One element of `hilight list`: `{net OUT 0}`, `{instance R1 1}`; anything
// else is a net name with colour 0.
HighlightEntry highlight_entry(const std::string& element) {
  const auto words = pcell::split_tcl_list(element);
  if (words && words.value().size() == 3) {
    const std::vector<std::string>& w = words.value();
    int colour = 0;
    const auto [end, ec] = std::from_chars(w[2].data(), w[2].data() + w[2].size(), colour);
    const bool number = ec == std::errc{} && end == w[2].data() + w[2].size();
    if (number && (w[0] == "net" || w[0] == "instance")) return HighlightEntry{w[1], w[0] == "instance", colour};
  }
  return HighlightEntry{element, false, 0};
}

}  // namespace

// --- properties ----------------------------------------------------------------------------

void PropertiesPanel::show_instance(std::string name) {
  target_ = PropertyTarget::instance;
  name_ = std::move(name);
  rows_.clear();
}

void PropertiesPanel::show_wire(std::size_t index) {
  target_ = PropertyTarget::wire;
  name_ = std::to_string(index);
  rows_.clear();
}

void PropertiesPanel::show_schematic() {
  target_ = PropertyTarget::schematic;
  name_.clear();
  rows_.clear();
}

void PropertiesPanel::clear() {
  target_ = PropertyTarget::none;
  name_.clear();
  rows_.clear();
}

std::string PropertiesPanel::title() const {
  switch (target_) {
    case PropertyTarget::instance: return name_;
    case PropertyTarget::wire: return "Wire " + name_;
    case PropertyTarget::schematic: return "Schematic";
    case PropertyTarget::none: break;
  }
  return {};
}

std::vector<std::string> PropertiesPanel::target_words() const {
  switch (target_) {
    case PropertyTarget::instance: return {"instance", name_};
    case PropertyTarget::wire: return {"wire", name_};
    case PropertyTarget::schematic: return {"schematic", "K"};
    case PropertyTarget::none: break;
  }
  return {};
}

std::optional<std::string> PropertiesPanel::load_command() const {
  if (target_ == PropertyTarget::none) return std::nullopt;
  std::vector<std::string> words{"getprop"};
  for (std::string& word : target_words()) words.push_back(std::move(word));
  return schematic(std::move(words));
}

void PropertiesPanel::set_properties(std::string_view property_string) {
  rows_.clear();
  for (const schem::Property& p : schem::parse_properties(property_string)) {
    rows_.push_back(PropertyRow{p.key, schem::decode_value(p.raw_value), p.has_value});
  }
}

std::optional<std::string> PropertiesPanel::set_command(std::string_view key, std::string_view value) const {
  if (key.empty() || (target_ != PropertyTarget::instance && target_ != PropertyTarget::wire)) return std::nullopt;
  std::vector<std::string> words{"setprop"};
  for (std::string& word : target_words()) words.push_back(std::move(word));
  words.emplace_back(key);
  words.emplace_back(value);
  return schematic(std::move(words));
}

std::optional<std::string> PropertiesPanel::remove_command(std::string_view key) const {
  if (key.empty() || (target_ != PropertyTarget::instance && target_ != PropertyTarget::wire)) return std::nullopt;
  std::vector<std::string> words{"setprop"};
  for (std::string& word : target_words()) words.push_back(std::move(word));
  words.emplace_back(key);
  return schematic(std::move(words));
}

// --- hierarchy breadcrumb ------------------------------------------------------------------

void HierarchyBreadcrumb::set_top(std::string label) {
  top_ = std::move(label);
}

void HierarchyBreadcrumb::set_path(std::string_view path) {
  elements_.clear();
  std::string current;
  int brackets = 0;
  for (const char c : path) {
    if (c == '.' && brackets == 0) {
      if (!current.empty()) elements_.push_back(std::move(current));
      current.clear();
      continue;
    }
    if (c == '[') ++brackets;
    if (c == ']' && brackets > 0) --brackets;
    current += c;
  }
  if (!current.empty()) elements_.push_back(std::move(current));
}

std::vector<Crumb> HierarchyBreadcrumb::crumbs() const {
  std::vector<Crumb> out;
  out.push_back(Crumb{top_, elements_.size()});
  for (std::size_t i = 0; i < elements_.size(); ++i) out.push_back(Crumb{elements_[i], elements_.size() - i - 1});
  return out;
}

std::string HierarchyBreadcrumb::text() const {
  std::string out = top_;
  for (const std::string& element : elements_) out += " › " + element;
  return out;
}

std::string HierarchyBreadcrumb::path_command() {
  return schematic({"hierarchy", "path"});
}

std::string HierarchyBreadcrumb::descend_command(std::string_view instance) {
  return schematic({"descend", std::string{instance}});
}

std::string HierarchyBreadcrumb::ascend_command() {
  return schematic({"ascend"});
}

std::vector<std::string> HierarchyBreadcrumb::click_commands(std::size_t index) const {
  const std::vector<Crumb> all = crumbs();
  if (index >= all.size()) return {};
  return std::vector<std::string>(all[index].ascents, ascend_command());
}

// --- net highlighting ----------------------------------------------------------------------

std::string NetHighlight::net_command(std::string_view net) {
  return schematic({"hilight", "net", std::string{net}});
}

std::string NetHighlight::instance_command(std::string_view instance) {
  return schematic({"hilight", "instance", std::string{instance}});
}

std::string NetHighlight::selection_command() {
  return schematic({"hilight", "selection"});
}

std::string NetHighlight::clear_command(bool all) {
  return all ? schematic({"hilight", "clear", "all"}) : schematic({"hilight", "clear"});
}

std::string NetHighlight::list_command() {
  return schematic({"hilight", "list"});
}

void NetHighlight::set_list(std::string_view output) {
  nets_.clear();
  instances_.clear();
  colours_.clear();
  const auto elements = pcell::split_tcl_list(output);
  if (!elements) return;
  for (const std::string& element : elements.value()) {
    const HighlightEntry entry = highlight_entry(element);
    (entry.instance ? instances_ : nets_).insert(entry.name);
    colours_.try_emplace({entry.instance, entry.name}, entry.colour);
  }
}

std::optional<int> NetHighlight::colour(std::string_view name, bool instance) const {
  const auto it = colours_.find(std::pair<bool, std::string>{instance, std::string{name}});
  return it == colours_.end() ? std::nullopt : std::optional<int>{it->second};
}

// --- messages ------------------------------------------------------------------------------

Message parse_message(std::string_view line) {
  Message m;
  std::string_view rest = line;
  const std::size_t colon = rest.find(": ");
  const std::string_view head = colon == std::string_view::npos ? std::string_view{} : rest.substr(0, colon);
  if (head == "error" || head == "warning" || head == "info" || head == "note") {
    m.severity = head == "error"     ? MessageSeverity::error
                 : head == "warning" ? MessageSeverity::warning
                                     : MessageSeverity::info;
    rest.remove_prefix(colon + 2);
  }
  const std::size_t code_end = rest.find(": ");
  if (code_end != std::string_view::npos && is_code(rest.substr(0, code_end))) {
    m.code = std::string{rest.substr(0, code_end)};
    rest.remove_prefix(code_end + 2);
  }
  m.text = std::string{rest};
  return m;
}

CommandOutcome outcome_of(const cmd::CommandResult& result) {
  CommandOutcome out;
  out.status = result.ok() ? OutcomeStatus::ok : OutcomeStatus::failed;
  for (const diag::Diagnostic& d : result.diagnostics) {
    if (d.severity == diag::Severity::debug) continue;
    out.diagnostics.push_back(diag::format_text(d));
    if (out.message.empty() && d.severity == diag::Severity::error) out.message = d.text;
  }
  out.output = result.value.to_string();
  return out;
}

void MessagesPanel::add(const CommandOutcome& outcome) {
  for (const std::string& line : outcome.diagnostics) messages_.push_back(parse_message(line));
  if (!outcome.ok() && outcome.diagnostics.empty() && !outcome.message.empty()) {
    messages_.push_back(Message{MessageSeverity::error, {}, outcome.message});
  }
  if (messages_.size() > kMaxMessages) {
    const auto excess = static_cast<std::ptrdiff_t>(messages_.size() - kMaxMessages);
    messages_.erase(messages_.begin(), messages_.begin() + excess);
  }
}

std::size_t MessagesPanel::count(MessageSeverity severity) const noexcept {
  return static_cast<std::size_t>(std::count_if(messages_.begin(), messages_.end(),
                                                [severity](const Message& m) { return m.severity == severity; }));
}

std::string MessagesPanel::summary() const {
  const std::size_t errors = count(MessageSeverity::error);
  const std::size_t warnings = count(MessageSeverity::warning);
  if (errors == 0 && warnings == 0) return messages_.empty() ? "No messages" : plural(messages_.size(), "message");
  if (warnings == 0) return plural(errors, "error");
  if (errors == 0) return plural(warnings, "warning");
  return plural(errors, "error") + " · " + plural(warnings, "warning");
}

// --- netlist action ------------------------------------------------------------------------

std::string netlist_command(const NetlistRequest& request) {
  std::vector<std::string> words{"netlist"};
  if (!request.file.empty()) words.push_back(request.file);
  switch (request.mode) {
    case NetlistMode::hier: words.emplace_back("-hier"); break;
    case NetlistMode::top: words.emplace_back("-top"); break;
    case NetlistMode::flat: words.emplace_back("-flat"); break;
  }
  if (request.lvs) words.emplace_back("-lvs");
  return schematic(std::move(words));
}

std::optional<std::string> NetlistAction::start(const NetlistRequest& request, std::string label,
                                                Clock::time_point now) {
  if (state_ == State::running) return std::nullopt;
  state_ = State::running;
  request_ = request;
  label_ = std::move(label);
  started_ = now;
  cancel_requested_ = false;
  status_.clear();
  text_.clear();
  return netlist_command(request);
}

void NetlistAction::finish(const CommandOutcome& outcome, MessagesPanel& messages) {
  if (state_ != State::running) return;
  messages.add(outcome);
  if (!outcome.ok()) {
    state_ = State::failed;
    status_ = "Netlist failed: " + (outcome.message.empty() ? std::string{"see Messages"} : outcome.message);
    return;
  }
  state_ = State::done;
  if (!request_.file.empty()) {
    status_ = "Netlist written: " + request_.file;
    return;
  }
  text_ = outcome.output;
  status_ = "Netlist ready (" + plural(line_count(text_), "line") + ")";
}

bool NetlistAction::request_cancel() noexcept {
  if (state_ != State::running || cancel_requested_) return false;
  cancel_requested_ = true;
  return true;
}

std::optional<InterruptCardView> NetlistAction::card(Clock::time_point now) const {
  if (state_ != State::running || now - started_ < kLongRunning) return std::nullopt;
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - started_).count();
  InterruptCardView view;
  view.title = "Netlisting " + label_ + " · " + std::to_string(seconds) + " s";
  view.detail = cancel_requested_ ? "Stopping: no file is written" : netlist_command(request_);
  return view;
}

}  // namespace hefesto::shell
