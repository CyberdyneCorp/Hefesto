// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/diag/codes.hpp"

#include <algorithm>
#include <array>
#include <vector>

#include "diag/area_codes.hpp"

namespace hefesto::diag {

namespace {

using enum Severity;

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array kTable = {
    CodeInfo{"CLI-LOG-001", warning, "The session log file cannot be opened; the run continues without it."},
    CodeInfo{"CLI-SCRIPT-001", error, "A script file named on the command line cannot be read."},
    CodeInfo{"CLI-TECH-001", warning, "Retired in M1, never reused. Was: `-T <tech>` recorded but not loaded; `-T` now loads the technology at startup."},
    CodeInfo{"CLI-USAGE-001", error, "Invalid command-line option or option argument."},
    CodeInfo{"CMD-DISPATCH-001", error, "Unknown command name; the command was not run."},
    CodeInfo{"CMD-INTERNAL-001", error, "A command handler raised an unexpected internal error; the command failed."},
    CodeInfo{"CMD-PARSE-001", error, "Retired in M4, never reused. Was: command script has an unterminated double quote (M0 command-script reader; lines are Tcl now)."},
    CodeInfo{"CMD-PARSE-002", error, "Retired in M4, never reused. Was: command script has an unterminated brace group (M0 command-script reader; lines are Tcl now)."},
    CodeInfo{"CMD-PARSE-003", error, "Retired in M4, never reused. Was: extra characters after a closing quote or brace in a command script (M0 command-script reader; lines are Tcl now)."},
    CodeInfo{"CMD-REGISTRY-001", error, "A command with the same name is already registered."},
    CodeInfo{"CMD-SESSION-001", error, "The command needs an editor session (technology, cells, windows) and none is attached."},
    CodeInfo{"CMD-USAGE-001", error, "Wrong number or form of command arguments."},
    CodeInfo{"CORE-NOTIMPL-001", error, "Retired in M1, never reused. Was: a declared feature not implemented yet (M1 scaffolding stub)."},
    CodeInfo{"CORE-NOTIMPL-002", error, "Retired in M2, never reused. Was: not implemented yet (an M2 command or function registered by the M2 scaffold before its workstream landed it)."},
    CodeInfo{"CORE-NOTIMPL-003", error, "Retired in M3, never reused. Was: not implemented yet (a `drc` option or M3 function stubbed by the M3 scaffold before its workstream landed it)."},
    CodeInfo{"CORE-NOTIMPL-004", error, "Retired in M4, never reused. Was: not implemented yet (an M4 command or function registered by the M4 scaffold before its workstream landed it)."},
    CodeInfo{"PLATFORM-IO-001", error, "A file cannot be read."},
    CodeInfo{"PLATFORM-IO-002", error, "A file cannot be written."},
    CodeInfo{"PLATFORM-IO-003", error, "A directory cannot be created, listed or removed."},
    CodeInfo{"PLATFORM-IO-004", error, "A file or directory does not exist."},
    CodeInfo{"PLATFORM-LOG-001", warning, "The session log cannot be opened; the session continues without it."},
    CodeInfo{"PLATFORM-PDK-001", error, "A technology was not found in any PDK or technology root."},
    CodeInfo{"PLATFORM-PROCESS-001", error, "An external process could not be started or waited for."},
    CodeInfo{"PLATFORM-UNSUPPORTED-001", error, "The requested platform service is not available on this platform."},
    CodeInfo{"SETTINGS-FILE-001", error, "A project settings file is malformed."},
    CodeInfo{"SETTINGS-NAME-001", error, "Unknown project setting name."},
    CodeInfo{"SETTINGS-VALUE-001", error, "Invalid value for a project setting."},
};

constexpr bool is_upper_alnum(char c) noexcept {
  return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

constexpr bool is_digit(char c) noexcept {
  return c >= '0' && c <= '9';
}

// True when `part` is non-empty and made of upper-case letters and digits.
constexpr bool is_word(std::string_view part) noexcept {
  return !part.empty() && std::all_of(part.begin(), part.end(), is_upper_alnum);
}

const std::array<CodeArea, 19>& areas() noexcept {
  static const std::array<CodeArea, 19> table = {
      CodeArea{"core", "", "docs/diagnostics/codes.md", kTable},
      CodeArea{"tech", "TECH", "docs/diagnostics/codes/tech.md", area_codes::tech()},
      CodeArea{"db", "DB", "docs/diagnostics/codes/db.md", area_codes::db()},
      CodeArea{"cell", "CELL", "docs/diagnostics/codes/cell.md", area_codes::cell()},
      CodeArea{"display", "DISPLAY", "docs/diagnostics/codes/display.md", area_codes::display()},
      CodeArea{"view", "VIEW", "docs/diagnostics/codes/view.md", area_codes::view()},
      CodeArea{"pdk", "PDK", "docs/diagnostics/codes/pdk.md", area_codes::pdk()},
      CodeArea{"app", "APP", "docs/diagnostics/codes/app.md", area_codes::app()},
      CodeArea{"undo", "UNDO", "docs/diagnostics/codes/undo.md", area_codes::undo()},
      CodeArea{"select", "SELECT", "docs/diagnostics/codes/select.md", area_codes::select()},
      CodeArea{"connect", "CONNECT", "docs/diagnostics/codes/connect.md", area_codes::connect()},
      CodeArea{"ui", "UI", "docs/diagnostics/codes/ui.md", area_codes::ui()},
      CodeArea{"wiring", "WIRE", "docs/diagnostics/codes/wire.md", area_codes::wiring()},
      // M3 areas (openspec/changes/m3-drc/design.md D1).
      CodeArea{"drc", "DRC", "docs/diagnostics/codes/drc.md", area_codes::drc()},
      CodeArea{"hints", "HINTS", "docs/diagnostics/codes/hints.md", area_codes::hints()},
      // M4 areas (openspec/changes/m4-scripting/design.md D1).
      CodeArea{"script", "SCRIPT", "docs/diagnostics/codes/script.md", area_codes::script()},
      CodeArea{"pcell", "PCELL", "docs/diagnostics/codes/pcell.md", area_codes::pcell()},
      CodeArea{"tools", "TOOLS", "docs/diagnostics/codes/tools.md", area_codes::tools()},
      CodeArea{"gui", "GUI", "docs/diagnostics/codes/gui.md", area_codes::gui()},
  };
  return table;
}

// Every code of every area, sorted by code (built once).
const std::vector<CodeInfo>& merged() {
  static const std::vector<CodeInfo> all = [] {
    std::vector<CodeInfo> codes;
    for (const auto& area : areas()) {
      codes.insert(codes.end(), area.codes.begin(), area.codes.end());
    }
    std::sort(codes.begin(), codes.end(), [](const CodeInfo& a, const CodeInfo& b) { return a.code < b.code; });
    return codes;
  }();
  return all;
}

std::string render_rows(std::span<const CodeInfo> codes) {
  std::string out = "| Code | Default severity | Meaning |\n|---|---|---|\n";
  for (const auto& info : codes) {
    out += "| `";
    out += info.code;
    out += "` | ";
    out += severity_name(info.severity);
    out += " | ";
    out += info.summary;
    out += " |\n";
  }
  return out;
}

}  // namespace

std::span<const CodeInfo> code_table() noexcept {
  return merged();
}

std::span<const CodeArea> code_areas() noexcept {
  return areas();
}

std::optional<CodeArea> find_code_area(std::string_view name) noexcept {
  for (const auto& area : areas()) {
    if (area.name == name) {
      return area;
    }
  }
  return std::nullopt;
}

std::optional<CodeInfo> find_code(std::string_view code) noexcept {
  const auto& table = merged();
  const auto it = std::lower_bound(table.begin(), table.end(), code,
                                    [](const CodeInfo& info, std::string_view c) { return info.code < c; });
  if (it == table.end() || it->code != code) {
    return std::nullopt;
  }
  return *it;
}

bool is_well_formed_code(std::string_view code) noexcept {
  const auto first = code.find('-');
  const auto last = code.rfind('-');
  if (first == std::string_view::npos || first == last) {
    return false;
  }
  const auto number = code.substr(last + 1);
  return is_word(code.substr(0, first)) && is_word(code.substr(first + 1, last - first - 1)) &&
         number.size() == 3 && std::all_of(number.begin(), number.end(), is_digit);
}

std::string render_code_table_markdown() {
  return render_rows(kTable);
}

std::string render_code_table_markdown(std::string_view area) {
  const auto found = find_code_area(area);
  return render_rows(found ? found->codes : std::span<const CodeInfo>{});
}

}  // namespace hefesto::diag
