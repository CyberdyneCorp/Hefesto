// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The panels of the desktop schematic editor around its canvas (board
// docs/design/mockups/Schematic.dc.html; openspec/changes/m8-schematic
// design D15; docs/design/m8-ui-conformance.md):
//
// - the properties panel (schematic-editing "Property editing"): it reads
//   the selected object's property string with `schematic getprop` and
//   writes one attribute with `schematic setprop` (no value removes it);
// - the hierarchy breadcrumb ("Hierarchy navigation", "Hierarchy path"):
//   the top schematic and one crumb per path element of
//   `schematic hierarchy path`; `schematic descend <instance>` goes down and
//   a click on a crumb issues one `schematic ascend` per level above it;
// - net highlighting ("Net highlighting"): `schematic hilight ...` lines and
//   the highlighted set read back from `schematic hilight list`;
// - the netlist action (schematic-command "Subcommand table", `netlist`):
//   the command line, the running state with the M4/M5 Interrupt card after
//   kLongRunning, and the result;
// - the messages panel: the diagnostics of every schematic command, newest
//   last, with their severity and code.
//
// Every action is a `schematic ...` command line issued through the
// window's CommandSink, so it is echoed, logged and replayable ("Command
// logging"); the panels only parse the results. Raven-free and unit-tested
// in tests/unit/shell/schematic_panels_test.cpp.
//
// Owner: SHELL (openspec/changes/m8-schematic design D3, D15; task 13.2).

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/shell/command_sink.hpp"
#include "hefesto/shell/script_consent_model.hpp"

namespace hefesto::shell {

// --- properties ----------------------------------------------------------------------------

/// What the properties panel shows.
enum class PropertyTarget : std::uint8_t { none, instance, wire, schematic };

struct PropertyRow {
  std::string key;
  std::string value;  // decoded ("Property strings")
  bool has_value = false;
};

class PropertiesPanel {
 public:
  void show_instance(std::string name);
  void show_wire(std::size_t index);
  void show_schematic();
  void clear();

  [[nodiscard]] PropertyTarget target() const noexcept { return target_; }
  /// "XM2", "Wire 3", "Schematic", empty for none.
  [[nodiscard]] std::string title() const;

  /// The command that reads the target's property string; nullopt for none.
  [[nodiscard]] std::optional<std::string> load_command() const;
  /// The result of load_command: the rows, in property-string order.
  void set_properties(std::string_view property_string);
  [[nodiscard]] const std::vector<PropertyRow>& rows() const noexcept { return rows_; }

  /// The command that sets `key` to `value` on the target (an instance or a
  /// wire); nullopt for none or the schematic, and for an empty key.
  [[nodiscard]] std::optional<std::string> set_command(std::string_view key, std::string_view value) const;
  /// The command that removes `key` (setprop without a value).
  [[nodiscard]] std::optional<std::string> remove_command(std::string_view key) const;

 private:
  [[nodiscard]] std::vector<std::string> target_words() const;

  PropertyTarget target_ = PropertyTarget::none;
  std::string name_;  // the instance name, or the wire index
  std::vector<PropertyRow> rows_;
};

// --- hierarchy breadcrumb ------------------------------------------------------------------

struct Crumb {
  std::string label;
  std::size_t ascents = 0;  // `schematic ascend` lines a click issues
};

class HierarchyBreadcrumb {
 public:
  /// The top schematic's label (its file name, "tb_inverter.sch").
  void set_top(std::string label);
  /// The result of `schematic hierarchy path` (".x1.xinv[2]."): the
  /// elements between the dots; brackets keep their dots.
  void set_path(std::string_view path);

  [[nodiscard]] std::vector<Crumb> crumbs() const;
  [[nodiscard]] std::size_t depth() const noexcept { return elements_.size(); }
  /// The crumbs joined for a narrow window: "tb_inverter.sch › x1 › xinv[2]".
  [[nodiscard]] std::string text() const;

  [[nodiscard]] static std::string path_command();
  [[nodiscard]] static std::string descend_command(std::string_view instance);
  [[nodiscard]] static std::string ascend_command();
  /// The lines a click on crumb `index` issues (none for the last crumb).
  [[nodiscard]] std::vector<std::string> click_commands(std::size_t index) const;

 private:
  std::string top_;
  std::vector<std::string> elements_;
};

// --- net highlighting ----------------------------------------------------------------------

class NetHighlight {
 public:
  [[nodiscard]] static std::string net_command(std::string_view net);
  [[nodiscard]] static std::string instance_command(std::string_view instance);
  [[nodiscard]] static std::string selection_command();
  /// `schematic hilight clear` (the current level), `... clear all` with `all`.
  [[nodiscard]] static std::string clear_command(bool all);
  [[nodiscard]] static std::string list_command();

  /// The result of list_command (a Tcl list of net names).
  void set_list(std::string_view output);
  [[nodiscard]] const std::set<std::string>& nets() const noexcept { return nets_; }

 private:
  std::set<std::string> nets_;
};

// --- messages ------------------------------------------------------------------------------

enum class MessageSeverity : std::uint8_t { info, warning, error };

struct Message {
  MessageSeverity severity = MessageSeverity::info;
  std::string code;  // "SCHEM-NET-004", empty when the line has none
  std::string text;
};

/// One diagnostic line of a CommandOutcome ("error: SCHEM-CMD-005: text").
[[nodiscard]] Message parse_message(std::string_view line);

class MessagesPanel {
 public:
  static constexpr std::size_t kMaxMessages = 1000;  // oldest dropped first

  /// Adds the outcome's diagnostics, and its message when it failed
  /// without any.
  void add(const CommandOutcome& outcome);
  void clear() noexcept { messages_.clear(); }
  [[nodiscard]] const std::vector<Message>& messages() const noexcept { return messages_; }
  [[nodiscard]] std::size_t count(MessageSeverity severity) const noexcept;
  /// "2 errors · 1 warning", "No messages".
  [[nodiscard]] std::string summary() const;

 private:
  std::vector<Message> messages_;
};

// --- netlist action ------------------------------------------------------------------------

enum class NetlistMode : std::uint8_t { hier, top, flat };

struct NetlistRequest {
  std::string file;  // empty: the text is returned, not written
  NetlistMode mode = NetlistMode::hier;
  bool lvs = false;
};

[[nodiscard]] std::string netlist_command(const NetlistRequest& request);

class NetlistAction {
 public:
  enum class State : std::uint8_t { idle, running, done, failed };
  using Clock = std::chrono::steady_clock;

  /// Starts a run of `request` for the document `label` ("inv.sch"); the
  /// command line to issue; nullopt while a run is going on.
  [[nodiscard]] std::optional<std::string> start(const NetlistRequest& request, std::string label,
                                                 Clock::time_point now);
  /// The end of the run: its outcome (diagnostics go to `messages`).
  void finish(const CommandOutcome& outcome, MessagesPanel& messages);
  /// Interrupt (the card's button, Ctrl-C): recorded for the card; the
  /// window interrupts the command through the script interrupt.
  void request_cancel() noexcept;

  [[nodiscard]] State state() const noexcept { return state_; }
  [[nodiscard]] bool enabled() const noexcept { return state_ != State::running; }
  /// The Interrupt card once the run took kLongRunning: "Netlisting inv.sch
  /// · 3 s", detail "Stopping: no file is written" after Interrupt.
  [[nodiscard]] std::optional<InterruptCardView> card(Clock::time_point now) const;
  /// The status line after a run: "Netlist written: inv.spice", "Netlist
  /// ready (12 lines)", "Netlist failed: <message>"; empty otherwise.
  [[nodiscard]] const std::string& status() const noexcept { return status_; }
  /// The text of a run without a file.
  [[nodiscard]] const std::string& text() const noexcept { return text_; }

 private:
  State state_ = State::idle;
  NetlistRequest request_;
  std::string label_;
  Clock::time_point started_{};
  bool cancel_requested_ = false;
  std::string status_;
  std::string text_;
};

}  // namespace hefesto::shell
