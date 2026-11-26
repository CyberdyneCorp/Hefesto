// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The property check of one matched device pair (lvs-property-matching
// "Properties of interest", "Tolerance rules", "Missing properties", "When
// properties are checked", "Pin-associated properties", "Property error
// results").
//
// Owner: LVSPROP.

#include "hefesto/lvs/prop/properties.hpp"

#include <cmath>
#include <optional>
#include <string>
#include <utility>

#include "hefesto/lvs/net/permute.hpp"
#include "lvs/prop/records.hpp"

namespace hefesto::lvs {

namespace {

using prop::Key;
using prop::Keys;
using prop::Network;
using prop::Record;
using prop::Value;

std::string_view printed_name(std::string_view name) {
  return !name.empty() && name.front() == '/' ? name.substr(1) : name;
}

std::string_view open_pin_text(OpenPin state) {
  return state == OpenPin::tied_together ? "tied together" : "no connects";
}

OpenPin open_pin(const Instance& instance, std::size_t pin) {
  return pin < instance.open_pins.size() ? instance.open_pins[pin] : OpenPin::none;
}

class Checker {
 public:
  Checker(const DeviceSide& first, const DeviceSide& second, const PropertySettings& settings)
      : first_(first),
        second_(second),
        settings_(settings),
        keys_(prop::build_keys(*first.klass, *second.klass, settings.case_insensitive)) {}

  PairCheck run() {
    check_open_pins();
    Network one = prop::build_network(*first_.instance, keys_);
    Network two = prop::build_network(*second_.instance, keys_);
    if (one.empty() || two.empty()) {
      check_missing_records(one, two);
      return std::move(out_);
    }
    if (one.size() > 1 || two.size() > 1) {
      if (prop::reconcile(one, two, keys_, settings_) > 1) {
        out_.console.emplace_back("No more changes can be made to series/parallel networks.");
      }
    }
    compare_with_retry(std::move(one), std::move(two));
    return std::move(out_);
  }

 private:
  void mismatch(std::string text, const std::string& key) {
    if (!header_) {
      out_.report.push_back(std::string{printed_name(first_.name)} + " vs. " +
                            std::string{printed_name(second_.name)} + ":");
      header_ = true;
    }
    out_.errors.push_back(PropertyError{std::string{first_.name}, std::string{second_.name}, key, text});
    out_.report.push_back(std::move(text));
    out_.outcome = PropertyOutcome::mismatch;
  }

  // ---------------------------------------------------------- open pins

  /// Whether the open-pin states agree pin by pin, with the second side's
  /// pins `p` and `q` exchanged when `swap` is set.
  [[nodiscard]] std::optional<std::size_t> open_pin_disagreement(std::optional<std::pair<std::uint32_t, std::uint32_t>> swap) const {
    const std::size_t pins = std::min(first_.instance->connections.size(), second_.instance->connections.size());
    for (std::size_t i = 0; i < pins; ++i) {
      std::size_t j = i;
      if (swap && i == swap->first) {
        j = swap->second;
      } else if (swap && i == swap->second) {
        j = swap->first;
      }
      const OpenPin a = open_pin(*first_.instance, i);
      const OpenPin b = open_pin(*second_.instance, j);
      if (a != OpenPin::none && b != OpenPin::none && a != b) {
        return i;
      }
    }
    return std::nullopt;
  }

  void check_open_pins() {
    const auto straight = open_pin_disagreement(std::nullopt);
    if (!straight) {
      return;
    }
    const auto pairs = permute_pairs(*first_.klass).pairs;
    if (!pairs.empty() && !open_pin_disagreement(pairs.front())) {
      return;
    }
    const std::size_t pin = *straight;
    const auto pin_name = [](const DeviceSide& side, std::size_t i) {
      return i < side.klass->ports.size() ? side.klass->ports[i].name : std::to_string(i + 1);
    };
    const auto line = [&](int circuit, const DeviceSide& side) {
      const OpenPin state = open_pin(*side.instance, pin);
      return "    Circuit" + std::to_string(circuit) + " instance " + std::string{printed_name(side.name)} +
             " pin " + pin_name(side, pin) + " connections are " + std::string{open_pin_text(state)} + " (" +
             std::to_string(static_cast<int>(state)) + ")";
    };
    mismatch("  Parallelized instances disagree on pin connections.", pin_name(first_, pin));
    out_.report.push_back(line(1, first_));
    out_.report.push_back(line(2, second_));
  }

  // ---------------------------------------------------- missing records

  void check_missing_records(const Network& one, const Network& two) {
    if (one.empty() && two.empty()) {
      return;
    }
    const bool first_has = !one.empty();
    const Network& present = first_has ? one : two;
    const DeviceSide& side = first_has ? first_ : second_;
    std::vector<bool> reported(keys_.list.size(), false);
    for (const auto& record : present) {
      for (std::size_t i = 0; i < keys_.list.size(); ++i) {
        const bool counts = i == Keys::kM || i == Keys::kS;
        if (!record.values[i].present || reported[i] || (counts && prop::count_of(record, i) == 1)) {
          continue;
        }
        reported[i] = true;
        mismatch(std::string{"Circuit "} + (first_has ? "1 " : "2 ") + side.klass->name + " instance " +
                     std::string{printed_name(side.name)} + " property \"" + keys_.list[i].name +
                     "\" has no match in circuit " + (first_has ? "2." : "1."),
                 keys_.list[i].name);
      }
    }
  }

  // ------------------------------------------------------- one to one

  [[nodiscard]] bool swapped(const Key& key) const {
    const auto pin = key.pin.empty() ? std::nullopt : port_position(*first_.klass, key.pin);
    if (!pin || !key.swap_partner) {
      return false;
    }
    const auto other = port_position(*first_.klass, keys_.list[*key.swap_partner].pin);
    const auto& k1 = first_.pin_keys;
    const auto& k2 = second_.pin_keys;
    if (!other || *pin >= k1.size() || *other >= k1.size() || *pin >= k2.size() || *other >= k2.size()) {
      return false;
    }
    return k1[*pin] != k1[*other] && k1[*pin] == k2[*other] && k1[*other] == k2[*pin];
  }

  /// The lines of one key's mismatch, by the promoted types.
  [[nodiscard]] std::string mismatch_line(const Key& key, const Value& a, const Value& b) const {
    if (a.type == PropertyType::expression || b.type == PropertyType::expression) {
      return " " + key.name + " (unresolved expressions.)";
    }
    if (prop::normal_type(a) != prop::normal_type(b)) {
      return " " + key.name + " (property type mismatch)";
    }
    const std::string head = " " + key.name + " circuit1: ";
    if (a.type == PropertyType::string) {
      const long prefix = std::lround(key.tolerance);
      return head + "\"" + a.text + "\"   circuit2: \"" + b.text + "\"   " +
             (prefix <= 0 ? std::string{"(exact match req'd)"} : "(check to " + std::to_string(prefix) + " chars.)");
    }
    std::string line = head + prop::value_text(a) + "   circuit2: " + prop::value_text(b) + "   ";
    if (a.as_double() > 0 && b.as_double() > 0) {
      if (a.type == PropertyType::integer) {
        line += "(delta=" + std::to_string(std::llabs(a.integer - b.integer)) + ", cutoff=" + format_g(key.tolerance) + ")";
      } else {
        const double ratio = relative_delta(a.number, b.number, settings_.netgen_compat);
        line += "(delta=" + format_g(100.0 * ratio, 3) + "%, cutoff=" + format_g(100.0 * key.tolerance, 3) + "%)";
      }
    }
    return line;
  }

  struct Outcome {
    std::vector<std::pair<std::string, std::string>> lines;  // (text, key)
    bool counts = false;  // an M or S mismatch, or a record-count mismatch
  };

  /// M and S: integers with tolerance 0, a missing one counting 1.
  void compare_count(std::size_t i, const Record& r1, const Record& r2, Outcome& result) const {
    if (prop::count_of(r1, i) == prop::count_of(r2, i)) {
      return;
    }
    const Key& key = keys_.list[i];
    const Value& a = r1.values[i].present ? r1.values[i] : key.fallback;
    const Value& b = r2.values[i].present ? r2.values[i] : key.fallback;
    result.lines.emplace_back(mismatch_line(key, a, b), key.name);
    result.counts = true;
  }

  void compare_key(std::size_t i, const Record& r1, const Record& r2, Outcome& result) const {
    const Key& key = keys_.list[i];
    const bool counts = i == Keys::kM || i == Keys::kS;
    const Value& a = r1.values[i];
    const Value& b = swapped(key) ? r2.values[*key.swap_partner] : r2.values[i];
    if (counts) {
      compare_count(i, r1, r2, result);
      return;
    }
    if (!a.present && !b.present) {
      return;
    }
    if (!a.present || !b.present) {
      result.lines.emplace_back(std::string{"Property "} + key.name + " in circuit" + (a.present ? "1" : "2") +
                                    " has no matching property in circuit" + (a.present ? "2" : "1"),
                                key.name);
      return;
    }
    if (!prop::values_match(key, a, b, settings_)) {
      result.lines.emplace_back(mismatch_line(key, a, b), key.name);
    }
  }

  [[nodiscard]] Outcome compare_networks(const Network& one, const Network& two) const {
    Outcome result;
    if (one.size() != two.size()) {
      result.counts = true;
      return result;
    }
    for (std::size_t r = 0; r < one.size(); ++r) {
      for (std::size_t i = 0; i < keys_.list.size(); ++i) {
        compare_key(i, one[r], two[r], result);
      }
    }
    return result;
  }

  void dump(int circuit, const Network& network) {
    out_.report.push_back("Circuit " + std::to_string(circuit) + ":");
    for (const auto& record : network) {
      std::string line = " ";
      line += record.tag.empty() ? "" : "_tag=" + record.tag + " ";
      for (std::size_t i = 0; i < keys_.list.size(); ++i) {
        if (record.values[i].present) {
          line += keys_.list[i].name + "=" + prop::value_text(record.values[i]) + " ";
        }
      }
      out_.report.push_back(line);
    }
  }

  void compare_with_retry(Network one, Network two) {
    Outcome outcome = compare_networks(one, two);
    if (outcome.counts) {
      // Fold M (then S) into the additive properties and compare again.
      for (const bool series : {false, true}) {
        prop::fold(one, keys_, series, settings_);
        prop::fold(two, keys_, series, settings_);
      }
      outcome = compare_networks(one, two);
    }
    if (one.size() != two.size()) {
      const bool first_larger = one.size() > two.size();
      mismatch(first_larger ? "Circuit 1 parallel/series network does not match Circuit 2"
                            : "Circuit 2 parallel/series network does not match Circuit 1",
               "M");
      dump(1, one);
      dump(2, two);
      return;
    }
    for (auto& [text, key] : outcome.lines) {
      mismatch(std::move(text), key);
    }
  }

  const DeviceSide& first_;
  const DeviceSide& second_;
  const PropertySettings& settings_;
  Keys keys_;
  PairCheck out_;
  bool header_ = false;
};

}  // namespace

PairCheck check_device_pair(const DeviceSide& first, const DeviceSide& second, const PropertySettings& settings) {
  if (first.klass == nullptr || second.klass == nullptr || first.instance == nullptr || second.instance == nullptr) {
    // Q-LVS-10 (replicate): a check that cannot compare is a failure; the
    // structured result carries the detail.
    PairCheck check;
    check.outcome = PropertyOutcome::failure;
    check.errors.push_back(PropertyError{std::string{first.name}, std::string{second.name}, {},
                                         std::string{kPropertyCheckFailed}});
    return check;
  }
  return Checker{first, second, settings}.run();
}

}  // namespace hefesto::lvs
