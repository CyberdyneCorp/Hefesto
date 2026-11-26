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
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/shell/command_sink.hpp"
#include "hefesto/shell/script_consent_model.hpp"

namespace hefesto::editor {
struct EditorState;
}  // namespace hefesto::editor

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

  /// The result of list_command: a Tcl list of `{net <name> <colour>}` and
  /// `{instance <name> <colour>}` elements (schematic-command "Subcommand
  /// table", `hilight list`); an element that is not such a triple is a net
  /// name with colour 0. Text that is not a Tcl list clears the set.
  void set_list(std::string_view output);
  [[nodiscard]] const std::set<std::string>& nets() const noexcept { return nets_; }
  [[nodiscard]] const std::set<std::string>& instances() const noexcept { return instances_; }
  /// The colour index (0..7, kHighlightColours) of a highlighted net or
  /// instance; nullopt when it is not highlighted.
  [[nodiscard]] std::optional<int> colour(std::string_view name, bool instance) const;

 private:
  std::set<std::string> nets_;
  std::set<std::string> instances_;
  std::map<std::pair<bool, std::string>, int, std::less<>> colours_;  // (instance, name) -> colour
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

/// A command's result as the panels see it, for a `schematic` command the
/// view observes through the dispatcher (cmd::CommandObserver): failed
/// when the command failed, its first error's text as the message, each
/// diagnostic rendered as diag::format_text ("error: SCHEM-NET-004: text"),
/// its value as the output. Debug diagnostics are left out.
[[nodiscard]] CommandOutcome outcome_of(const cmd::CommandResult& result);

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
  /// Interrupt (the card's button, Ctrl-C) while running: recorded for the
  /// card. True the first time only: the window then issues the script
  /// interrupt once (a second interrupt request is not sent again, as a
  /// second Ctrl-C would mean more than a cancel).
  bool request_cancel() noexcept;
  [[nodiscard]] bool cancel_requested() const noexcept { return cancel_requested_; }

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

/// The top bar's "Netlist" button (board: next to Simulate; M8 task 13.2):
/// the same action as the netlist bar and the `n` binding.
struct NetlistButtonView {
  std::string label;
  std::string tooltip;
  bool enabled = false;
};
[[nodiscard]] NetlistButtonView netlist_button(const NetlistAction& action, bool has_document);

/// Runs one netlist action through `run` (the window's command sink; the
/// view's dispatcher observer calls `action.finish` with the result): the
/// top bar's button, the netlist bar and the `n` binding all come here.
/// With no document, a message says so; while a run is going on, nothing
/// happens. A run the sink did not finish (the command never reached the
/// dispatcher) ends as failed "the netlist command did not run". False
/// when no run started.
bool run_netlist(NetlistAction& action, bool has_document, std::string label,
                 const std::function<void(const std::string&)>& run, MessagesPanel& messages);

/// The schematic tab's Interrupt (the card's button, Ctrl-C). While the
/// netlist runs, the first press records the cancel and sets the editor's
/// pending Interrupt (ui::request_interrupt), the flag the run polls between
/// hierarchy levels and in every `tcleval` evaluation (SCHEM-CMD-011), and
/// interrupts the console interpreter; true then. With no run, the press
/// is the editor's ordinary Interrupt; false.
bool interrupt_schematic(NetlistAction& action, editor::EditorState& editor);

}  // namespace hefesto::shell
