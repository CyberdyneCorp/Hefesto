// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// comp.out, the console text, the structured list result and the result
// classification (lvs-reports "Log and console routing", "Structured list
// result", "Result classification"). comp.out is the run's text that is not
// console-only, byte for byte; the object references of PairReport are
// never written ("Layout object references in structured results").
//
// Owner: LVSREP.

#include "hefesto/lvs/report/report.hpp"

#include <cstdint>

namespace hefesto::lvs {

diag::Outcome<std::string> write_comp_out(const LvsResult& result) {
  std::string out;
  for (const auto& text : result.output) {
    if (!text.console_only) {
      out += text.text;
    }
  }
  return out;
}

std::string console_text(const LvsResult& result, bool logging) {
  std::string out;
  for (std::size_t i = 0; i < result.output.size(); ++i) {
    const auto& text = result.output[i];
    if (!logging || text.console_only || i >= result.final_output) {
      out += text.text;
    }
  }
  return out;
}

std::string json_file_name(std::string_view log) {
  const auto slash = log.find_last_of('/');
  const auto dot = log.find_last_of('.');
  if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) {
    return std::string{log} + ".json";
  }
  return std::string{log.substr(0, dot)} + ".json";
}

OverallStatus overall_status(const LvsResult& result) noexcept {
  if (result.input_error) {
    return OverallStatus::error;
  }
  if (result.cancelled) {
    return OverallStatus::cancelled;
  }
  return result.clean ? OverallStatus::clean : OverallStatus::mismatch;
}

std::string_view overall_status_name(OverallStatus status) noexcept {
  switch (status) {
    case OverallStatus::clean:
      return "clean";
    case OverallStatus::mismatch:
      return "mismatch";
    case OverallStatus::error:
      return "error";
    case OverallStatus::cancelled:
      return "cancelled";
  }
  return "error";
}

std::string_view pair_status_name(PairStatus status) noexcept {
  switch (status) {
    case PairStatus::match:
      return "match";
    case PairStatus::match_property_errors:
      return "match-property-errors";
    case PairStatus::match_port_errors:
      return "match-port-errors";
    case PairStatus::match_port_property_errors:
      return "match-port-property-errors";
    case PairStatus::symmetric:
      return "symmetric";
    case PairStatus::black_box:
      return "black-box";
    case PairStatus::mismatch:
      return "mismatch";
  }
  return "mismatch";
}

namespace {

using List = cmd::Value::List;

cmd::Value count(std::uint64_t n) {
  return cmd::Value{static_cast<std::int64_t>(n)};
}

cmd::Value devices(const std::vector<DeviceCount>& side) {
  List list;
  for (const auto& d : side) {
    list.push_back(List{d.name, count(d.count)});
  }
  return list;
}

cmd::Value nets(const std::vector<NetFragment>& side) {
  List list;
  for (const auto& f : side) {
    List entries;
    for (const auto& e : f.entries) {
      entries.push_back(List{e.device_class, e.pin, count(e.count)});
    }
    list.push_back(List{f.name, std::move(entries)});
  }
  return list;
}

cmd::Value instances(const std::vector<InstanceFragment>& side) {
  List list;
  for (const auto& f : side) {
    List entries;
    for (const auto& pin : f.pins) {
      for (std::size_t k = 0; k < pin.pins.size(); ++k) {
        entries.push_back(List{pin.pins[k], count(k < pin.fanouts.size() ? pin.fanouts[k] : 0)});
      }
    }
    list.push_back(List{f.name, std::move(entries)});
  }
  return list;
}

template <typename Fragment, typename Side>
cmd::Value fragments(const std::vector<MismatchClass<Fragment>>& classes, const Side& side) {
  List list;
  for (const auto& cls : classes) {
    list.push_back(List{side(cls.first), side(cls.second)});
  }
  return list;
}

cmd::Value properties(const std::vector<PropertyPair>& pairs) {
  List list;
  for (const auto& p : pairs) {
    List entry{List{p.device1, p.device2}};
    for (std::size_t k = 0; k < p.values1.size() && k < p.values2.size(); ++k) {
      entry.push_back(List{List{p.values1[k].key, p.values1[k].value}, List{p.values2[k].key, p.values2[k].value}});
    }
    list.push_back(std::move(entry));
  }
  return list;
}

cmd::Value pair_list(const PairReport& r) {
  List keys;
  const auto add = [&keys](const char* key, cmd::Value value) {
    keys.emplace_back(key);
    keys.push_back(std::move(value));
  };
  if (r.has_devices) {
    add("name", List{r.cell1, r.cell2});
    add("devices", List{devices(r.devices1), devices(r.devices2)});
    add("nets", List{count(r.nets1), count(r.nets2)});
  }
  if (r.converged) {
    add("badnets", fragments(r.bad_nets, nets));
    add("badelements", fragments(r.bad_elements, instances));
  }
  if (!r.properties.empty()) {
    add("properties", properties(r.properties));
  }
  if (r.pins) {
    List pins1(r.pins->first.begin(), r.pins->first.end());
    List pins2(r.pins->second.begin(), r.pins->second.end());
    add("pins", List{std::move(pins1), std::move(pins2)});
  }
  return keys;
}

}  // namespace

diag::Outcome<cmd::Value> to_list(const LvsResult& result) {
  List list;
  for (const auto& report : result.reports) {
    list.push_back(pair_list(report));
  }
  return cmd::Value{std::move(list)};
}

}  // namespace hefesto::lvs
