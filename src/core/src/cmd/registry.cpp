// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/cmd/registry.hpp"

#include <algorithm>
#include <cctype>
#include <span>

#include "hefesto/diag/codes.hpp"
#include "hefesto/script/binding/keyword_rule.hpp"

namespace hefesto::cmd {

namespace {

// The global window commands of "Command registration and help text"
// (windows/windMain.c:472-553, windows/windClient.c:680-828; Tcl build).
constexpr std::string_view kWindowCommands[] = {
    "*bypass",     "*crash",      "*files",           "*grstats",         "*pause",      "*winddebug",
    "*winddump",   "center",      "closewindow",      "cursor",           "grow",        "help",
    "imacro",      "logcommands", "macro",            "openwindow",       "over",        "pushbutton",
    "quit",        "redo",        "redraw",           "reset",            "scroll",      "setpoint",
    "sleep",       "specialopen", "underneath",       "undo",             "updatedisplay", "version",
    "view",        "windowborder", "windowcaption",   "windownames",      "windowpositions",
    "windowscrollbars", "xview",  "zoom"};

// Hefesto commands that belong to the interpreter rather than to a window
// client: Magic's Tcl build has them as Tcl commands or tools.tcl procs, so
// they are reachable from every window.
constexpr std::string_view kInterpreterCommands[] = {
    "autoincr", "cancelselect", "clearstack", "echo",      "genkey",    "history",  "keepselect",
    "measure",  "peekbox",      "popbox",     "popstack",  "pushbox",   "pushstack", "resumeall",
    "ruler",    "send",         "setting",    "source",    "startselect", "suspendall", "tool",
    "trackwire", "unmeasure"};

bool listed(std::span<const std::string_view> names, std::string_view name) {
  return std::find(names.begin(), names.end(), name) != names.end();
}

// Magic's Lookup() character rule: letters match case-insensitively, any
// other character exactly.
bool same_char(char table, char word) {
  const auto t = static_cast<unsigned char>(table);
  const auto w = static_cast<unsigned char>(word);
  return std::tolower(t) == std::tolower(w);
}

enum class EntryMatch : std::uint8_t { none, prefix, exact };

EntryMatch match_entry(std::string_view word, std::string_view entry) {
  const std::string_view name = keyword_of(entry);  // Q-SCRIPT-14 (keyword_rule.hpp)
  if (word.size() > name.size()) {
    return EntryMatch::none;
  }
  for (std::size_t i = 0; i < word.size(); ++i) {
    if (!same_char(name[i], word[i])) {
      return EntryMatch::none;
    }
  }
  return word.size() == name.size() ? EntryMatch::exact : EntryMatch::prefix;
}

}  // namespace

std::string_view strip_namespace(std::string_view word) {
  for (const std::string_view prefix : {std::string_view{"::magic::"}, std::string_view{"magic::"}}) {
    if (word.size() >= prefix.size() && word.substr(0, prefix.size()) == prefix) {
      return word.substr(prefix.size());
    }
  }
  return word;
}

NameMatch match_name(std::string_view word, const std::vector<std::string_view>& names) {
  const std::string_view stripped = strip_namespace(word);
  NameMatch result;
  for (std::size_t i = 0; i < names.size(); ++i) {
    const EntryMatch m = match_entry(stripped, names[i]);
    if (m == EntryMatch::exact) {
      return NameMatch{LookupStatus::found, i};
    }
    if (m == EntryMatch::prefix) {
      result = result.status == LookupStatus::not_found ? NameMatch{LookupStatus::found, i}
                                                         : NameMatch{LookupStatus::ambiguous, 0};
    }
  }
  return result;
}

std::optional<diag::Diagnostic> CommandRegistry::add(CommandSpec spec) {
  const std::string client{default_client(spec.name)};
  return add(std::move(spec), client);
}

std::optional<diag::Diagnostic> CommandRegistry::add(CommandSpec spec, std::string_view client) {
  auto table = tables_.find(client);
  if (table == tables_.end()) {
    table = tables_.emplace(std::string{client}, Table{}).first;
  }
  std::string name = spec.name;
  const auto [it, inserted] = table->second.try_emplace(std::move(name), std::move(spec));
  if (!inserted) {
    return diag::make(diag::codes::kCmdRegistryDuplicate, "command '" + it->first + "' is already registered");
  }
  return std::nullopt;
}

void CommandRegistry::replace(CommandSpec spec, std::string_view client) {
  auto table = tables_.find(client);
  if (table == tables_.end()) {
    table = tables_.emplace(std::string{client}, Table{}).first;
  }
  std::string name = spec.name;
  table->second.insert_or_assign(std::move(name), std::move(spec));
}

const CommandRegistry::Table* CommandRegistry::table_of(std::string_view client) const {
  const auto it = tables_.find(client);
  return it == tables_.end() ? nullptr : &it->second;
}

const CommandSpec* CommandRegistry::find(std::string_view name, std::string_view client) const {
  const Table* table = table_of(client);
  if (table == nullptr) {
    return nullptr;
  }
  const auto it = table->find(name);
  return it == table->end() ? nullptr : &it->second;
}

const CommandSpec* CommandRegistry::find(std::string_view name) const {
  if (const auto* spec = find(name, kWindowClient)) {
    return spec;
  }
  if (const auto* spec = find(name, kLayoutClient)) {
    return spec;
  }
  for (const auto& [client, table] : tables_) {
    if (const auto it = table.find(name); it != table.end()) {
      return &it->second;
    }
  }
  return nullptr;
}

CommandLookup CommandRegistry::lookup(std::string_view word, std::string_view client, NameLookup mode) const {
  const Table* table = table_of(client);
  if (table == nullptr) {
    return {};
  }
  if (mode == NameLookup::exact) {
    const auto it = table->find(strip_namespace(word));
    return it == table->end() ? CommandLookup{} : CommandLookup{LookupStatus::found, &it->second, std::string{client}};
  }
  std::vector<std::string_view> names;
  std::vector<const CommandSpec*> specs;
  names.reserve(table->size());
  specs.reserve(table->size());
  for (const auto& [name, spec] : *table) {
    names.push_back(name);
    specs.push_back(&spec);
  }
  const NameMatch m = match_name(word, names);
  if (m.status != LookupStatus::found) {
    return CommandLookup{m.status, nullptr, {}};
  }
  return CommandLookup{LookupStatus::found, specs[m.index], std::string{client}};
}

CommandLookup CommandRegistry::resolve(std::string_view word, std::string_view client, bool window_commands,
                                       NameLookup mode) const {
  const CommandLookup global = window_commands ? lookup(word, kWindowClient, mode) : CommandLookup{};
  // windSend.c: "If both command tables point to window commands, only do
  // the command once."
  const CommandLookup own = client == kWindowClient ? CommandLookup{} : lookup(word, client, mode);
  if (global.status == LookupStatus::ambiguous || own.status == LookupStatus::ambiguous) {
    return CommandLookup{LookupStatus::ambiguous, nullptr, {}};
  }
  if (global.status == LookupStatus::not_found) {
    if (own.status == LookupStatus::not_found) {
      // Tcl-side procedures: exact names only (strip_namespace applied by lookup).
      return lookup(word, kScriptClient, NameLookup::exact);
    }
    return own;
  }
  if (own.status == LookupStatus::not_found) {
    return global;
  }
  // Both matched: compare just the two entries; the client's command wins
  // unless only the global one is an exact name.
  const NameMatch m = match_name(word, {own.spec->name, global.spec->name});
  if (m.status == LookupStatus::ambiguous) {
    return CommandLookup{LookupStatus::ambiguous, nullptr, {}};
  }
  return m.index == 0 ? own : global;
}

std::vector<std::string> CommandRegistry::names() const {
  std::vector<std::string> out;
  for (const auto& [client, table] : tables_) {
    for (const auto& kv : table) {
      out.push_back(kv.first);
    }
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

std::vector<const CommandSpec*> CommandRegistry::table(std::string_view client) const {
  std::vector<const CommandSpec*> out;
  const Table* t = table_of(client);
  if (t == nullptr) {
    return out;
  }
  for (const auto& kv : *t) {
    out.push_back(&kv.second);
  }
  std::stable_sort(out.begin(), out.end(), [](const CommandSpec* a, const CommandSpec* b) {
    return help_string(*a) < help_string(*b);
  });
  return out;
}

std::vector<std::string> CommandRegistry::clients() const {
  std::vector<std::string> out;
  out.reserve(tables_.size());
  for (const auto& kv : tables_) {
    out.push_back(kv.first);
  }
  return out;
}

std::string_view CommandRegistry::default_client(std::string_view name) {
  if (listed(kWindowCommands, name) || listed(kInterpreterCommands, name) || name.starts_with("hefesto::")) {
    return kWindowClient;
  }
  return kLayoutClient;
}

std::string CommandRegistry::help_string(const CommandSpec& spec) {
  const std::string_view synopsis = spec.synopsis;
  const bool named = synopsis.starts_with(spec.name) &&
                     (synopsis.size() == spec.name.size() || synopsis[spec.name.size()] == ' ' ||
                      synopsis[spec.name.size()] == '\n');
  if (named) {
    return spec.synopsis;
  }
  return synopsis.empty() ? spec.name : spec.name + " " + spec.synopsis;
}

}  // namespace hefesto::cmd
