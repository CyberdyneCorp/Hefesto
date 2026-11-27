// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// comp.json (lvs-reports "JSON file"): netgen's line layout and
// indentation, with Q-LVS-13's fixes in both netgen-compat modes: every
// string is a JSON string (quote, backslash and control characters
// escaped), and an empty list is written as its opening and closing lines,
// which is valid JSON.
//
// Owner: LVSREP.

#include <cstdio>
#include <string>
#include <vector>

#include "hefesto/lvs/report/report.hpp"

namespace hefesto::lvs {

namespace {

std::string json_string(std::string_view text) {
  std::string out = "\"";
  for (const char c : text) {
    const auto byte = static_cast<unsigned char>(c);
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (byte < 0x20) {
      char buffer[8];
      std::snprintf(buffer, sizeof buffer, "\\u%04x", byte);
      out += buffer;
    } else {
      out += c;
    }
  }
  return out + "\"";
}

std::string comma(bool more) {
  return more ? ",\n" : "\n";
}

class JsonWriter {
 public:
  std::string write(const std::vector<PairReport>& reports) {
    out_ = "[\n";
    for (std::size_t i = 0; i < reports.size(); ++i) {
      pair(reports[i], i + 1 < reports.size());
    }
    out_ += "]\n";
    return std::move(out_);
  }

 private:
  /// The keys of one pair, in order, each written by its function.
  void pair(const PairReport& r, bool more) {
    std::vector<void (JsonWriter::*)(const PairReport&)> keys;
    if (r.has_devices) {
      keys.insert(keys.end(), {&JsonWriter::name, &JsonWriter::devices, &JsonWriter::nets});
    }
    if (r.converged) {
      keys.insert(keys.end(), {&JsonWriter::badnets, &JsonWriter::badelements});
    }
    if (!r.properties.empty()) {
      keys.push_back(&JsonWriter::properties);
    }
    if (r.pins) {
      keys.push_back(&JsonWriter::pins);
    }
    out_ += "  {\n";
    for (std::size_t k = 0; k < keys.size(); ++k) {
      (this->*keys[k])(r);
      out_ += "   ]" + comma(k + 1 < keys.size());
    }
    out_ += "  }" + comma(more);
  }

  void name(const PairReport& r) {
    out_ += "   \"name\": [\n";
    out_ += "      " + json_string(r.cell1) + ",\n";
    out_ += "      " + json_string(r.cell2) + "\n";
  }

  void device_side(const std::vector<DeviceCount>& side, bool first) {
    for (std::size_t i = 0; i < side.size(); ++i) {
      const bool last = i + 1 == side.size();
      const std::string close = first && !last ? "]," : (last ? " ]" : " ],");
      out_ += "         [" + json_string(side[i].name) + ", " + std::to_string(side[i].count) + close + "\n";
    }
  }

  void devices(const PairReport& r) {
    out_ += "   \"devices\": [\n";
    out_ += "       [\n";
    device_side(r.devices1, true);
    out_ += "       ], [\n";
    device_side(r.devices2, false);
    out_ += "       ]\n";
  }

  void nets(const PairReport& r) {
    out_ += "   \"nets\": [\n";
    out_ += "    " + std::to_string(r.nets1) + ",\n";
    out_ += "    " + std::to_string(r.nets2) + "\n";
  }

  template <typename Fragment, typename Entry>
  void fragment_side(const std::vector<Fragment>& side, const Entry& entries) {
    for (std::size_t i = 0; i < side.size(); ++i) {
      out_ += "          [\n";
      out_ += "            " + json_string(side[i].name) + ",\n";
      out_ += "            [\n";
      const auto lines = entries(side[i]);
      for (std::size_t k = 0; k < lines.size(); ++k) {
        out_ += "              [ " + lines[k] + " ]" + comma(k + 1 < lines.size());
      }
      out_ += "            ]\n";
      out_ += "          ]" + comma(i + 1 < side.size());
    }
  }

  template <typename Fragment, typename Entry>
  void fragment_classes(const char* key, const std::vector<MismatchClass<Fragment>>& classes, const Entry& entries) {
    out_ += std::string{"   \""} + key + "\": [\n";
    for (std::size_t c = 0; c < classes.size(); ++c) {
      out_ += "      [\n";
      out_ += "        [\n";
      fragment_side(classes[c].first, entries);
      out_ += "        ], [\n";
      fragment_side(classes[c].second, entries);
      out_ += "        ]\n";
      out_ += "      ]" + comma(c + 1 < classes.size());
    }
  }

  void badnets(const PairReport& r) {
    fragment_classes("badnets", r.bad_nets, [](const NetFragment& f) {
      std::vector<std::string> lines;
      for (const auto& e : f.entries) {
        lines.push_back(json_string(e.device_class) + ", " + json_string(e.pin) + ", " + std::to_string(e.count));
      }
      return lines;
    });
  }

  void badelements(const PairReport& r) {
    fragment_classes("badelements", r.bad_elements, [](const InstanceFragment& f) {
      std::vector<std::string> lines;
      for (const auto& pin : f.pins) {
        for (std::size_t k = 0; k < pin.pins.size(); ++k) {
          const auto n = k < pin.fanouts.size() ? pin.fanouts[k] : 0;
          lines.push_back(json_string(pin.pins[k]) + ", " + std::to_string(n));
        }
      }
      return lines;
    });
  }

  void property_side(const std::string& device, const std::vector<PropertyValue>& values, bool more) {
    out_ += "         [\n";
    out_ += "           " + json_string(device) + ",\n";
    out_ += "           [\n";
    for (std::size_t k = 0; k < values.size(); ++k) {
      out_ += "             [" + json_string(values[k].key) + ", " + json_string(values[k].value) + "]" +
              comma(k + 1 < values.size());
    }
    out_ += "           ]\n";
    out_ += "         ]" + comma(more);
  }

  void properties(const PairReport& r) {
    out_ += "   \"properties\": [\n";
    for (std::size_t i = 0; i < r.properties.size(); ++i) {
      const auto& p = r.properties[i];
      out_ += "      [\n";
      property_side(p.device1, p.values1, true);
      property_side(p.device2, p.values2, false);
      out_ += "      ]" + comma(i + 1 < r.properties.size());
    }
  }

  void pin_side(const std::vector<std::string>& side) {
    for (std::size_t k = 0; k < side.size(); ++k) {
      out_ += "        " + json_string(side[k]) + comma(k + 1 < side.size());
    }
  }

  void pins(const PairReport& r) {
    out_ += "   \"pins\": [\n";
    out_ += "      [\n";
    pin_side(r.pins->first);
    out_ += "      ], [\n";
    pin_side(r.pins->second);
    out_ += "      ]\n";
  }

  std::string out_;
};

}  // namespace

diag::Outcome<std::string> write_json(const LvsResult& result) {
  return JsonWriter{}.write(result.reports);
}

}  // namespace hefesto::lvs
