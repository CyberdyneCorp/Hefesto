// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs::Session: the LVS state of one session, editor::EditorState::lvs
// (openspec/changes/m9-lvs-simulation design.md D4).
//
// Owner: SCAFFOLD owns the struct; each member belongs to the named
// workstream.

#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "hefesto/lvs/hier/run.hpp"

namespace hefesto::netlist {
struct NameMap;  // netlist/name_map.hpp (MAPS)
}

namespace hefesto::lvs {

struct Session {
  // LVSCMD: the last result per document (keyed by the layout cell or the
  // schematic path of circuit 1), for the browser and the status chip.
  std::map<std::string, LvsResult, std::less<>> last_results;

  // LVSCMD: the document of the run in progress (nullopt when idle) and its
  // cancel flag (read through CancelToken).
  std::optional<std::string> running;
  std::shared_ptr<std::atomic<bool>> cancel = std::make_shared<std::atomic<bool>>(false);

  // LVSCMD: the name maps of the two circuits of the last run (nullptr for
  // a netlist file), for cross-probing.
  std::shared_ptr<const netlist::NameMap> names1;
  std::shared_ptr<const netlist::NameMap> names2;

  // LVSCMD (M9 R3): the input files of the last run per document with
  // their modification times, so that the status chip turns `LVS stale`
  // when one changes ("LVS status chip").
  struct InputStamp {
    std::string path;
    std::optional<std::filesystem::file_time_type> modified;

    friend bool operator==(const InputStamp&, const InputStamp&) = default;
  };
  std::map<std::string, std::vector<InputStamp>, std::less<>> inputs;
  /// LVSCMD (M9 R3): documents whose layout changed after their last run;
  /// the layout edit hook adds the document (an INTEGRATION open item), a
  /// new run removes it.
  std::set<std::string, std::less<>> changed;
  /// LVSCMD (M9 R3): the exit status of the last run for the headless tool
  /// ("Headless LVS": 0 clean, 1 other completed comparison, 2 input error).
  std::optional<int> headless_status;
};

}  // namespace hefesto::lvs
