// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cross-PDK helper warning (Q-SCRIPT-41; design D7).

#include "hefesto/script/pdk/helper_origins.hpp"

#include <cstdint>
#include <string>

#include "hefesto/script/pdk/trust.hpp"

namespace hefesto::script {

namespace {

// After each successful `proc`: the procedure's fully qualified name (from
// the caller's namespace), the file being sourced and the line, handed to
// the native recorder. Errors in the hook never disturb the script.
constexpr std::string_view kHookScript = R"tcl(
namespace eval ::hefesto::pdk {}
proc ::hefesto::pdk::proc_defined {command code result op} {
    if {$code != 0} {return}
    set file [info script]
    if {$file eq ""} {return}
    catch {
        set name [uplevel 1 [list namespace which -command [lindex $command 1]]]
        set line 0
        for {set level [expr {[info frame] - 1}]} {$level > 0} {incr level -1} {
            set frame [info frame $level]
            if {[dict exists $frame file] && [dict exists $frame line]} {
                set line [dict get $frame line]
                break
            }
        }
        ::hefesto::pdk::record_proc $name $file $line
    }
}
trace add execution ::proc leave ::hefesto::pdk::proc_defined
)tcl";

std::uint32_t line_number(std::string_view text) {
  std::uint32_t line = 0;
  for (const char c : text) {
    if (c < '0' || c > '9' || line > 100'000'000U) {
      return 0;
    }
    line = line * 10U + static_cast<std::uint32_t>(c - '0');
  }
  return line;
}

}  // namespace

std::optional<HelperRedefinition> HelperOriginTracker::record(std::string_view procedure, const PdkIdentity& pdk,
                                                              const ScriptLocation& location) {
  const auto it = origins_.find(procedure);
  if (it == origins_.end()) {
    origins_.emplace(std::string{procedure}, pdk);
    return std::nullopt;
  }
  if (it->second == pdk) {
    return std::nullopt;
  }
  HelperRedefinition out{std::string{procedure}, it->second, pdk, location};
  it->second = pdk;
  return out;
}

std::optional<PdkIdentity> HelperOriginTracker::origin(std::string_view procedure) const {
  const auto it = origins_.find(procedure);
  if (it == origins_.end()) {
    return std::nullopt;
  }
  return it->second;
}

diag::Diagnostic redefinition_warning(const HelperRedefinition& redefinition) {
  std::string name = redefinition.procedure;
  if (name.starts_with("::")) {
    name.erase(0, 2);
  }
  return diag::make("SCRIPT-PDK-003",
                    "Warning: " + name + " defined by the " + redefinition.first.variant + " scripts (" +
                        redefinition.first.to_string() + ") is redefined by the " + redefinition.second.variant +
                        " scripts (" + redefinition.second.to_string() + ")",
                    redefinition.location);
}

cmd::CommandResult install_helper_origin_hook(Interpreter& interp, HelperOriginTracker& tracker,
                                              ScriptEvents& events, diag::DiagnosticSink* sink) {
  interp.define_command("::hefesto::pdk::record_proc", [&tracker, &events, sink](const cmd::Invocation& inv) {
    if (inv.args.size() != 3 || !inv.args[0].starts_with("::magic::")) {
      return cmd::CommandResult::success();
    }
    const auto pdk = pdk_identity_of(platform::Path{inv.args[1]});
    if (!pdk) {
      return cmd::CommandResult::success();
    }
    const ScriptLocation location{inv.args[1], line_number(inv.args[2])};
    if (const auto redefined = tracker.record(inv.args[0], *pdk, location)) {
      events.publish_helper_redefined(*redefined);
      if (sink != nullptr) {
        sink->emit(redefinition_warning(*redefined));
      }
    }
    return cmd::CommandResult::success();
  });
  return interp.evaluate(kHookScript, cmd::Origin::internal);
}

}  // namespace hefesto::script
