// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The extresist input model (extresist/output/output.hpp read_input();
// capability extresist-command "Input model"): ResReadExt's own record
// handling over EXTFLAT's tokenizer (extflat/text.hpp), and
// ResReadParentExt's drive points from the parents' `connect` records.
//
// Q-NETLIST-29 (compat) applies to `attr` records (see output.hpp). Beyond
// Magic, in both magic-compat settings (crashes and undefined behaviour):
// a negative node resistance is +infinity (Magic converts INFINITY to an
// int); a device terminal naming a net with no record yet creates the net
// (Magic dereferences a null net); a device whose model the style does not
// declare has no tile type (Magic dereferences a null device line); a
// `substrate` record with fewer than 7 fields and a parent `connect` with
// fewer than 8 are ignored (Magic reads past argv); a parent `use` whose
// 2x2 part is not Manhattan is ignored, and its transform is inverted and
// applied in 64 bits, saturated to 32 (Magic's int arithmetic overflows).
//
// Ported from Magic 8.3.694 (fc5daa9e) resis/ResReadExt.c, Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: RESCMD.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>

#include "extflat/text.hpp"
#include "extresist/output/output_codes.hpp"
#include "hefesto/extresist/output/output.hpp"

namespace hefesto::extresist {

namespace {

namespace xt = extflat::detail;

constexpr std::size_t kMaxLine = std::size_t{16} << 20;

std::string rect_text(const db::Rect& r) {
  return std::to_string(r.x0) + " " + std::to_string(r.y0) + " " + std::to_string(r.x1) + " " +
         std::to_string(r.y1);
}

// StrIsInt: an optional sign and digits only.
bool is_int(std::string_view s) {
  if (!s.empty() && (s.front() == '-' || s.front() == '+')) {
    s.remove_prefix(1);
  }
  return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

// StrIsNumeric: the whole token parses as a number.
bool is_numeric(const std::string& s) {
  if (s.empty()) {
    return false;
  }
  char* end = nullptr;
  (void)std::strtod(s.c_str(), &end);
  return end != nullptr && *end == '\0';
}

// Minimum field counts beyond the shared token table (Magic reads past argv).
constexpr std::size_t kSubstrateFields = 7;
constexpr std::size_t kParentConnectFields = 8;

std::int32_t sat32(std::int64_t v) {
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(v, std::numeric_limits<std::int32_t>::min(),
                                                            std::numeric_limits<std::int32_t>::max()));
}

bool unit(std::int32_t v) { return v == 1 || v == -1; }

// The 2x2 part is a rotation or reflection by a multiple of 90 degrees.
bool manhattan(const db::Transform& t) {
  return (t.b == 0 && t.d == 0 && unit(t.a) && unit(t.e)) || (t.a == 0 && t.e == 0 && unit(t.b) && unit(t.d));
}

// db::invert in 64 bits, saturated.
db::Transform invert_saturated(const db::Transform& t) {
  const std::int64_t c = -(std::int64_t{t.a} * t.c + std::int64_t{t.d} * t.f);
  const std::int64_t f = -(std::int64_t{t.b} * t.c + std::int64_t{t.e} * t.f);
  return db::Transform{t.a, t.d, sat32(c), t.b, t.e, sat32(f)};
}

db::Point apply_saturated(const db::Transform& t, std::int64_t x, std::int64_t y) {
  return db::Point{sat32(t.a * x + t.b * y + t.c), sat32(t.d * x + t.e * y + t.f)};
}

// db::apply on a rectangle in 64 bits, saturated.
db::Rect apply_saturated(const db::Transform& t, const db::Rect& r) {
  const db::Point p0 = apply_saturated(t, r.x0, r.y0);
  const db::Point p1 = apply_saturated(t, r.x1, r.y1);
  return db::canonical(db::Rect{p0.x, p0.y, p1.x, p1.y});
}

class Reader {
 public:
  Reader(std::string cell, const InputOptions& options) : options_(options) { model_.cell = std::move(cell); }

  void read(std::string_view text, std::string_view file) {
    file_ = file;
    xt::LineReader lines(text, kMaxLine);
    while (const auto line = lines.next()) {
      line_ = line->number;
      record(line->tokens);
    }
  }

  void read_parent(std::string_view text, std::string_view file) {
    file_ = file;
    uses_.clear();
    xt::LineReader lines(text, kMaxLine);
    while (const auto line = lines.next()) {
      line_ = line->number;
      parent_record(line->tokens);
    }
  }

  // The lists are appended to while reading (a prepend per record would
  // make a net with N terminals cost O(N^2)); Magic prepends, so they are
  // reversed once here into its newest-first order.
  InputModel take() {
    for (InputNode& n : model_.nodes) {
      std::reverse(n.drive_points.begin(), n.drive_points.end());
      std::reverse(n.sink_points.begin(), n.sink_points.end());
      std::reverse(n.devices.begin(), n.devices.end());
    }
    return std::move(model_);
  }

 private:
  void message(std::string text) const {
    if (options_.emit) {
      options_.emit(diag::make(output_codes::kMessage, std::move(text)));
    }
  }
  void report(std::string text) const {
    if (options_.emit) {
      options_.emit(diag::make(output_codes::kReport, std::move(text)));
    }
  }
  void read_error(const std::string& text) const {
    message(std::string{file_} + ", line " + std::to_string(line_) + ": " + text);
  }

  // LookupStruct and the minimum token count.
  std::optional<xt::Key> key(const std::vector<std::string>& t) const {
    const auto info = xt::lookup_key(t.front());
    if (!info) {
      read_error("Unrecognized token \"" + t.front() + "\" (ignored)");
      return std::nullopt;
    }
    if (t.size() < info->min_tokens) {
      read_error("Not enough tokens for " + t.front() + " line");
      return std::nullopt;
    }
    return info->key;
  }

  void record(const std::vector<std::string>& t) {
    const std::optional<xt::Key> k = key(t);
    if (!k) {
      return;
    }
    switch (*k) {
      case xt::Key::device:
        device(t);
        break;
      case xt::Key::fet:
        fet(t);
        break;
      case xt::Key::connect:
        connect_point(t);
        break;
      case xt::Key::port:
        port(t);
        break;
      case xt::Key::node:
        current_ = node(t);
        break;
      case xt::Key::substrate:
        if (t.size() < kSubstrateFields) {
          read_error("Not enough tokens for substrate line");
        } else {
          current_ = node(t);
        }
        break;
      case xt::Key::attr:
        attribute(t);
        break;
      case xt::Key::cap:
        capacitor(t);
        break;
      default:
        break;
    }
  }

  void parent_record(const std::vector<std::string>& t) {
    const std::optional<xt::Key> k = key(t);
    if (k == xt::Key::use) {
      use(t);
    } else if (k == xt::Key::connect && t.size() >= kParentConnectFields) {
      drive_point(t);
    } else if (k == xt::Key::connect) {
      read_error("Not enough tokens for connect line");
    }
  }

  // ResExtInitNode: the net of `name`, created at its first mention.
  std::size_t net(const std::string& name) {
    if (const auto found = index_.find(name); found != index_.end()) {
      return found->second;
    }
    InputNode n;
    n.name = name;
    model_.nodes.push_back(std::move(n));
    index_.emplace(name, model_.nodes.size() - 1);
    return model_.nodes.size() - 1;
  }

  std::optional<std::size_t> existing(const std::string& name) const {
    const auto found = index_.find(name);
    return found != index_.end() ? std::optional{found->second} : std::nullopt;
  }

  // DBTechNameType / DBTechNoisyNameType.
  std::optional<tech::TileType> type_of(const std::string& name, bool noisy) const {
    if (options_.tech == nullptr) {
      return std::nullopt;
    }
    const auto found = options_.tech->find_type(name);
    if (!found.ok() && noisy) {
      message(std::string{found.error == tech::LookupError::ambiguous ? "Ambiguous" : "Unrecognized"} +
              " layer (type) name \"" + name + "\"");
    }
    return found.id;
  }

  std::optional<std::size_t> node(const std::vector<std::string>& t) {
    const std::size_t i = net(t[1]);
    InputNode& n = model_.nodes[i];
    n.location = db::Point{xt::c_atoi(t[4]), xt::c_atoi(t[5])};
    n.type = t[6];
    n.tile_type = type_of(t[6], false);
    n.status.node_record = true;
    const std::int32_t r = xt::c_atoi(t[2]);
    const double scale = options_.style != nullptr ? static_cast<double>(static_cast<std::int32_t>(options_.style->rscale)) : 1.0;
    n.resistance = r < 0 ? std::numeric_limits<double>::infinity()
                         : static_cast<double>(static_cast<float>(r) * static_cast<float>(scale));
    n.node_cap = xt::c_atof(t[3]);
    n.cap += n.node_cap;
    if (!n.tile_type) {
      message("Bad tile type name in .ext file for node " + n.name);
      return std::nullopt;
    }
    return i;
  }

  void attribute(const std::vector<std::string>& t) {
    std::optional<std::size_t> target = current_;
    if (!options_.magic_compat) {
      // Q-NETLIST-29 OFF: the named node.
      target = existing(t[1]);
      if (!target) {
        message("Warning: attribute for unknown node " + t[1] + " ignored");
        return;
      }
    } else if (!target) {
      // Both modes: Magic reads an uninitialised node here.
      message("Warning: attribute " + t[1] + " before any node record ignored");
      return;
    }
    apply_attribute(model_.nodes[*target], t);
  }

  void apply_attribute(InputNode& n, const std::vector<std::string>& t) {
    const std::string& text = t[7];
    if (text.starts_with("res:skip")) {
      n.status.force ? message("Warning: Node " + t[1] + " is both forced and skipped") : void(n.status.skip = true);
    } else if (text.starts_with("res:force")) {
      n.status.skip ? message("Warning: Node " + t[1] + " is both skipped and forced ") : void(n.status.force = true);
    } else if (text.starts_with("res:min=")) {
      n.status.minsize = true;
      n.min_resistance = xt::c_atof(std::string_view{text}.substr(8));  // any length (Magic: 20 bytes)
    } else if (text.starts_with("res:drive") && options_.signal) {
      n.status.drive_located = true;
      const db::Point p{xt::c_atoi(t[2]), xt::c_atoi(t[3])};
      n.drive_points.push_back(ConnectPoint{db::Rect{p.x, p.y, p.x, p.y}, type_of(t[6], true)});
    }
  }

  void capacitor(const std::vector<std::string>& t) {
    const double value = xt::c_atof(t[3]);
    const std::size_t a = net(t[1]);
    const std::size_t b = net(t[2]);
    if (options_.signal) {
      model_.nodes[a].cap += value;
      model_.nodes[b].cap += value;
    } else {
      model_.nodes[a].coupling += value;
      model_.nodes[b].coupling += value;
    }
  }

  static db::Rect rect_at(const std::vector<std::string>& t, std::size_t first) {
    return db::Rect{xt::c_atoi(t[first]), xt::c_atoi(t[first + 1]), xt::c_atoi(t[first + 2]),
                    xt::c_atoi(t[first + 3])};
  }

  void debug_point(const char* what, const db::Rect& r) const {
    if (options_.debug) {
      report(std::string{"Added "} + what + " at " + rect_text(r));
    }
  }

  void port(const std::vector<std::string>& t) {
    InputNode& n = model_.nodes[net(t[1])];
    const ConnectPoint p{rect_at(t, 3), type_of(t[7], true)};
    n.drive_points.push_back(p);
    n.status.force = n.status.drive_located = n.status.port = true;
    debug_point("port", p.rect);
  }

  void connect_point(const std::vector<std::string>& t) {
    const std::optional<tech::TileType> type = type_of(t[5], true);
    if (!type) {
      message("Bad tile type name \"" + t[5] + "\" in .ext file for node " + t[6]);
      return;
    }
    if (t[6] == "None") {
      return;
    }
    const std::optional<std::size_t> i = existing(t[6]);
    if (!i) {
      message("Unknown node name \"" + t[6] + "\" in .ext file connect entry");
      return;
    }
    InputNode& n = model_.nodes[*i];
    const ConnectPoint p{rect_at(t, 1), type};
    n.sink_points.push_back(p);
    n.status.force = n.status.drive_located = true;
    debug_point("sink", p.rect);
  }

  // ResReadUse: a use of this cell in the parent.
  void use(const std::vector<std::string>& t) {
    if (t.size() < 9 || t[1] != model_.cell) {
      return;
    }
    const db::Transform tr{xt::c_atoi(t[3]), xt::c_atoi(t[4]), xt::c_atoi(t[5]),
                           xt::c_atoi(t[6]), xt::c_atoi(t[7]), xt::c_atoi(t[8])};
    if (manhattan(tr)) {
      uses_[t[2]] = invert_saturated(tr);
    }
  }

  // ResReadDrivePoint: "<use>/<node>" of a connection into this cell.
  void drive_point(const std::vector<std::string>& t) {
    std::string down = t[7];
    const std::size_t slash = down.find('/');
    const std::string id = down.substr(0, slash);
    const std::string name = slash == std::string::npos ? id : down.substr(slash + 1);
    const auto use = uses_.find(id);
    if (use == uses_.end()) {
      return;
    }
    const std::optional<tech::TileType> type = type_of(t[5], true);
    if (!type) {
      message("Bad tile type name \"" + t[5] + "\" in .ext file for node " + t[6]);
      return;
    }
    if (name == "None") {
      return;
    }
    const std::optional<std::size_t> i = existing(name);
    if (!i) {
      message("Unknown node name \"" + name + "\" in .ext file connect entry");
      return;
    }
    InputNode& n = model_.nodes[*i];
    const ConnectPoint p{apply_saturated(use->second, rect_at(t, 1)), type};
    n.drive_points.push_back(p);
    n.status.force = n.status.drive_located = true;
    debug_point("driver", p.rect);
  }

  // The style's device line for the model: its class and extGetDevType.
  void style_device(InputDevice& d) const {
    if (options_.style == nullptr) {
      return;
    }
    for (const extract::DeviceInfo& info : options_.style->device_info()) {
      if (info.model == d.model) {
        d.tile_type = info.type;
        break;
      }
    }
    for (const extract::DevicePattern& line : options_.style->devices) {
      if (line.model == d.model) {
        d.style_class = line.device_class;
        d.sd_count = line.terminal_count;
        // doPermute: exts_deviceSDTypes[1] empty or equal to [0].
        d.permute_sd = line.terminal_count > 1 &&
                       (line.terminals.size() < 2 || line.terminals[1].empty() || line.terminals[1] == line.terminals[0]);
        break;
      }
    }
  }

  // One terminal (net, length, attribute) of a device record.
  std::size_t terminal(std::size_t device, const std::string& name, Terminal role) {
    const std::size_t i = net(name);
    model_.nodes[i].devices.push_back(NodeDevice{device, role});
    return i;
  }

  void device(const std::vector<std::string>& t) {
    InputDevice d;
    d.device_class = t[1];
    d.model = t[2];
    d.location = db::Point{xt::c_atoi(t[3]), xt::c_atoi(t[4])};
    style_device(d);
    std::size_t i = 4;  // DEV_Y: the end of the numeric and parameter fields
    while (i < t.size() && (t[i].find('=') != std::string::npos || is_int(t[i]))) {
      ++i;
    }
    if (i == t.size()) {
      message("Bad device " + t[2] + ":  Too few arguments in .ext file");
      return;
    }
    d.leading.assign(t.begin() + std::min<std::ptrdiff_t>(7, static_cast<std::ptrdiff_t>(i)), t.begin() + static_cast<std::ptrdiff_t>(i));
    const std::size_t index = model_.devices.size();
    terminals(t, i, index, d);
    model_.devices.push_back(std::move(d));
  }

  void terminals(const std::vector<std::string>& t, std::size_t i, std::size_t index, InputDevice& d) {
    if ((t.size() - i) % 3 == 1) {
      if (t[i] != "None" && !is_numeric(t[i])) {
        d.subs = terminal(index, t[i], Terminal::subs);
      }
      ++i;
    }
    if (i + 2 >= t.size()) {
      return;
    }
    d.gate = terminal(index, t[i], Terminal::gate);
    d.gate_attr = t[i + 2];
    d.length = xt::c_atoi(t[i + 1]);
    i += 3;
    if (i + 2 < t.size()) {
      d.source = terminal(index, t[i], Terminal::source);
      d.source_attr = t[i + 2];
      d.width = xt::c_atoi(t[i + 1]);
      i += 3;
    }
    if (i + 2 < t.size()) {
      d.drain = terminal(index, t[i], Terminal::drain);
      d.drain_attr = t[i + 2];
      d.width = std::max(d.width, xt::c_atoi(t[i + 1]));
      i += 3;
    }
    if (i + 2 < t.size()) {
      message("Warning:  Device " + d.model + " has more than 4 ports (not handled).");
    }
    d.wl = d.length == 0 ? 0.0F : static_cast<float>(d.width) / static_cast<float>(d.length);
  }

  // ResReadFET: the terminals are not added to the nets' device lists.
  void fet(const std::vector<std::string>& t) {
    if (t.size() < 16) {
      return;
    }
    InputDevice d;
    d.device_class = "fet";
    d.model = t[1];
    d.location = db::Point{xt::c_atoi(t[2]), xt::c_atoi(t[3])};
    d.leading = {t[4], t[5]};
    style_device(d);
    d.gate = net(t[7]);
    d.source = net(t[10]);
    d.drain = net(t[13]);
    d.subs = net(t[6]);
    d.gate_attr = t[9];
    d.source_attr = t[12];
    d.drain_attr = t[15];
    d.length = xt::c_atoi(t[8]);
    d.width = std::max(xt::c_atoi(t[11]), xt::c_atoi(t[14]));
    d.wl = d.length == 0 ? 0.0F : static_cast<float>(d.width) / static_cast<float>(d.length);
    model_.devices.push_back(std::move(d));
  }

  const InputOptions& options_;
  InputModel model_;
  std::map<std::string, std::size_t, std::less<>> index_;
  std::map<std::string, db::Transform, std::less<>> uses_;
  std::optional<std::size_t> current_;  // the last node record read (curnode)
  std::string_view file_;
  std::uint32_t line_ = 0;
};

}  // namespace

std::vector<std::size_t> InputModel::net_order() const {
  std::vector<std::size_t> order(nodes.size());
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    order[i] = nodes.size() - 1 - i;
  }
  return order;
}

std::optional<std::size_t> InputModel::find(std::string_view name) const {
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].name == name) {
      return i;
    }
  }
  return std::nullopt;
}

DeviceAnnotations InputModel::annotations(std::size_t node) const {
  DeviceAnnotations out;
  if (node >= nodes.size()) {
    return out;
  }
  for (const NodeDevice& on : nodes[node].devices) {
    const InputDevice& d = devices[on.device];
    NetDevice nd;
    nd.name = d.model;
    nd.model = d.model;
    nd.location = d.location;
    nd.type = d.tile_type.value_or(tech::kSpace);
    for (const auto& term : {d.gate, d.source, d.drain, d.subs}) {
      if (term) {
        nd.terminals.push_back(*term);
      }
    }
    out.devices.push_back(std::move(nd));
  }
  return out;
}

diag::Outcome<InputModel> read_input(std::string_view cell, const InputOptions& options) {
  const std::optional<std::string> text = options.load ? options.load(cell) : std::nullopt;
  if (!text) {
    return diag::make(output_codes::kCannotOpen, "Cannot open file " + std::string{cell} + ".ext");
  }
  Reader reader{std::string{cell}, options};
  const std::string file = std::string{cell} + ".ext";
  reader.read(*text, file);
  for (const std::string& parent : options.parents) {
    const std::optional<std::string> parent_text = options.load(parent);
    if (!parent_text) {
      if (options.emit) {
        options.emit(diag::make(output_codes::kMessage, "Cannot open file " + parent + ".ext"));
      }
      continue;
    }
    reader.read_parent(*parent_text, parent + ".ext");
  }
  return reader.take();
}

}  // namespace hefesto::extresist
