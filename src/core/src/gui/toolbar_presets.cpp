// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The layer toolbar and its presets file (see toolbar_presets.hpp).
// (Magic ref: tcltk/toolbar.tcl:7-241, tcltk/reorderLayers.tcl:140-240)

#include "hefesto/gui/toolbar_presets.hpp"

#include <algorithm>
#include <array>

#include "gui/gui_util.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/gui/diag_codes.hpp"

namespace hefesto::gui {

namespace {

constexpr std::string_view kBlank = " \t";

bool is_blank(char c) {
  return c == ' ' || c == '\t';
}

std::string_view trim(std::string_view text) {
  const std::size_t start = text.find_first_not_of(kBlank);
  if (start == std::string_view::npos) {
    return {};
  }
  const std::size_t end = text.find_last_not_of(" \t\r");
  return text.substr(start, end - start + 1);
}

// The next blank-separated word of `text` from `pos` (pos moves past it).
std::string_view next_word(std::string_view text, std::size_t& pos) {
  while (pos < text.size() && is_blank(text[pos])) {
    ++pos;
  }
  const std::size_t start = pos;
  while (pos < text.size() && !is_blank(text[pos])) {
    ++pos;
  }
  return text.substr(start, pos - start);
}

std::vector<std::string> split_words(std::string_view text) {
  std::vector<std::string> out;
  std::size_t pos = 0;
  for (std::string_view word = next_word(text, pos); !word.empty(); word = next_word(text, pos)) {
    out.emplace_back(word);
  }
  return out;
}

// A `see` layer argument: names separated by commas or blanks.
std::vector<std::string> split_layers(std::string_view text) {
  std::string spaced{text};
  std::replace(spaced.begin(), spaced.end(), ',', ' ');
  return split_words(spaced);
}

bool plain_name(std::string_view name) {
  return !name.empty() && name.find_first_of("${}[]\"\\;") == std::string_view::npos;
}

// The layer list of `{...}` or `"..."` (the rest of the line), when it is
// plain data: no nesting, quoting, substitution or text after it.
std::optional<std::vector<std::string>> quoted_layers(std::string_view rest) {
  if (rest.size() < 2) {
    return std::nullopt;
  }
  const char open = rest.front();
  const char close = open == '{' ? '}' : open == '"' ? '"' : '\0';
  if (close == '\0' || rest.back() != close) {
    return std::nullopt;
  }
  const std::string_view inner = rest.substr(1, rest.size() - 2);
  const std::string_view forbidden = open == '{' ? std::string_view{"{}\\"} : std::string_view{"\"$[]\\{}"};
  if (inner.find_first_of(forbidden) != std::string_view::npos) {
    return std::nullopt;
  }
  return split_words(inner);
}

// `set <name> {<layers>}` or `set <name> "<layers>"`, or nullopt.
std::optional<ToolbarPreset> data_line(std::string_view line) {
  std::size_t pos = 0;
  if (next_word(line, pos) != "set") {
    return std::nullopt;
  }
  const std::string_view name = next_word(line, pos);
  if (!plain_name(name)) {
    return std::nullopt;
  }
  auto layers = quoted_layers(trim(line.substr(pos)));
  if (!layers) {
    return std::nullopt;
  }
  return ToolbarPreset{std::string{name}, std::move(*layers), 0};
}

// The lines of `text` (\n, \r\n or \r), without their ends.
std::vector<std::string_view> lines_of(std::string_view text) {
  std::vector<std::string_view> out;
  std::size_t pos = 0;
  while (pos < text.size()) {
    const std::size_t end = text.find_first_of("\r\n", pos);
    if (end == std::string_view::npos) {
      out.push_back(text.substr(pos));
      break;
    }
    out.push_back(text.substr(pos, end - pos));
    pos = end + 1 + ((text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n') ? 1U : 0U);
  }
  return out;
}

std::string joined(const std::vector<std::string>& words) {
  std::string out;
  for (const std::string& w : words) {
    out += (out.empty() ? "" : " ") + w;
  }
  return out;
}

// `./<tech>_toolbars.tcl` of the session, read and parsed; nullopt when absent.
struct LoadedPresets {
  std::string name;
  platform::Path path;
  std::string text;
  PresetFile file;
};

std::optional<LoadedPresets> read_presets(cmd::CommandContext& ctx) {
  const std::string tech = detail::run(ctx, {"tech", "name"});
  if (tech.find('/') != std::string::npos) {
    return std::nullopt;  // security finding S-15: the name must not lead out of the working directory
  }
  const std::string name = toolbar_presets_file_name(tech);
  const platform::Path path = detail::session_path(ctx, name);
  auto text = ctx.services().files.read(path);
  if (!text.ok()) {
    return std::nullopt;
  }
  PresetFile file = parse_toolbar_presets(text.value());
  return LoadedPresets{name, path, std::move(text).value(), std::move(file)};
}

}  // namespace

const ToolbarPreset* PresetFile::find(std::string_view name) const {
  const ToolbarPreset* found = nullptr;
  for (const ToolbarPreset& p : presets) {
    if (p.name == name) {
      found = &p;
    }
  }
  return found;
}

std::vector<std::string> PresetFile::names() const {
  std::vector<std::string> out;
  for (const ToolbarPreset& p : presets) {
    if (std::find(out.begin(), out.end(), p.name) == out.end()) {
      out.push_back(p.name);
    }
  }
  return out;
}

std::string toolbar_presets_file_name(std::string_view tech) {
  return std::string{tech} + "_toolbars.tcl";
}

PresetFile parse_toolbar_presets(std::string_view text) {
  PresetFile out;
  const std::vector<std::string_view> lines = lines_of(text);
  for (std::size_t i = 0; i < lines.size(); ++i) {
    const std::string_view line = trim(lines[i]);
    if (line.empty() || line.front() == '#') {
      continue;
    }
    if (auto preset = data_line(line)) {
      preset->line = i + 1;
      out.presets.push_back(std::move(*preset));
    } else {
      out.skipped.push_back(SkippedStatement{i + 1, std::string{line}});
    }
  }
  return out;
}

std::vector<diag::Diagnostic> skipped_diagnostics(const PresetFile& file, std::string_view file_name) {
  std::vector<diag::Diagnostic> out;
  for (const SkippedStatement& s : file.skipped) {
    out.push_back(diag::make_native(
        codes::kToolbarStatement, "Not read: only `set <name> {<layers>}` lines are read from the toolbar presets file: " + s.text,
        diag::SourceLocation{std::string{file_name}, static_cast<std::uint32_t>(s.line)}));
  }
  return out;
}

std::string rewrite_current_toolbar(std::string_view text, const std::vector<std::string>& layers) {
  static constexpr std::string_view kKey = "set current_toolbar {";
  const std::string replacement = "set current_toolbar {" + joined(layers) + "}";
  std::string out;
  std::size_t pos = 0;
  while (pos < text.size()) {
    const std::size_t end = text.find('\n', pos);
    const std::size_t stop = end == std::string_view::npos ? text.size() : end;
    std::string_view line = text.substr(pos, stop - pos);
    // regsub -line: the leftmost `set current_toolbar {`, greedy `.*` to the last `}` of the line.
    const std::size_t start = line.find(kKey);
    const std::size_t close = line.rfind('}');
    if (start != std::string_view::npos && close != std::string_view::npos && close >= start + kKey.size()) {
      out += std::string{line.substr(0, start)} + replacement + std::string{line.substr(close + 1)};
    } else {
      out += line;
    }
    if (end != std::string_view::npos) {
      out += '\n';
    }
    pos = stop + 1;
  }
  return out;
}

std::string write_current_toolbar(const ToolbarModel& toolbar) {
  return "set current_toolbar {" + joined(toolbar.layers) + "}\n";
}

ToolbarModel build_toolbar(cmd::CommandContext& ctx, const ToolbarOptions& options) {
  ToolbarModel model;
  if (!options.hide_special) {
    model.layers = {"errors", "labels", "subcell"};
  }
  const std::vector<std::string> tech = detail::words_of(
      detail::run_value(ctx, options.hide_locked ? std::vector<std::string>{"tech", "unlocked"}
                                                 : std::vector<std::string>{"tech", "layers", "*"}));
  model.layers.insert(model.layers.end(), tech.begin(), tech.end());
  model.locked = detail::words_of(detail::run_value(ctx, {"tech", "locked"}));
  if (const auto presets = read_presets(ctx)) {
    model.presets_file = presets->name;
    for (const diag::Diagnostic& d : skipped_diagnostics(presets->file, presets->name)) {
      ctx.emit(d);
    }
    if (const ToolbarPreset* current = presets->file.find("current_toolbar")) {
      model.layers = current->layers;
    }
  }
  return model;
}

std::vector<std::string> toolbar_preset_names(cmd::CommandContext& ctx) {
  const auto presets = read_presets(ctx);
  return presets ? presets->file.names() : std::vector<std::string>{"default_toolbar"};
}

diag::Outcome<std::vector<std::string>> load_toolbar_preset(cmd::CommandContext& ctx, std::string_view preset) {
  const auto presets = read_presets(ctx);
  if (!presets) {
    if (preset == "default_toolbar") {
      return build_toolbar(ctx, {}).layers;  // no file: the order applies only in memory
    }
    return diag::make(codes::kToolbarPreset,
                      "Selected preset " + std::string{preset} + " is not in the list of toolbar configurations, "
                      "inside " + toolbar_presets_file_name(detail::run(ctx, {"tech", "name"})) + ".");
  }
  const ToolbarPreset* found = presets->file.find(preset);
  if (found == nullptr) {
    return diag::make(codes::kToolbarPreset, "Selected preset " + std::string{preset} +
                                                 " is not in the list of toolbar configurations, inside " +
                                                 presets->name + ".");
  }
  const std::string text = rewrite_current_toolbar(presets->text, found->layers);
  if (text != presets->text && !ctx.services().files.write_atomic(presets->path, text).ok()) {
    return diag::make(codes::kToolbarPreset, "Cannot write the toolbar presets file " + presets->name + ".");
  }
  return found->layers;
}

std::optional<std::vector<std::string>> toolbar_row_command(std::string_view window, std::string_view layer,
                                                            bool locked, ToolbarAction action) {
  const std::string w{window};
  const std::string l{layer};
  switch (action) {
    case ToolbarAction::button1: return std::vector<std::string>{w, "see", l};
    case ToolbarAction::button3: return std::vector<std::string>{w, "see", "no", l};
    case ToolbarAction::key_u:
      return locked ? std::optional{std::vector<std::string>{w, "tech", "unlock", l}} : std::nullopt;
    default: break;
  }
  if (locked) {
    return std::nullopt;
  }
  switch (action) {
    case ToolbarAction::button2:
    case ToolbarAction::key_p: return std::vector<std::string>{w, "paint", l};
    case ToolbarAction::shift_button2:
    case ToolbarAction::key_e: return std::vector<std::string>{w, "erase", l};
    case ToolbarAction::key_s: return std::vector<std::string>{w, "select", "more", "area", l};
    case ToolbarAction::key_shift_s: return std::vector<std::string>{w, "select", "less", "area", l};
    case ToolbarAction::key_l: return std::vector<std::string>{w, "tech", "lock", l};
    default: return std::nullopt;
  }
}

std::string toolbar_row_message(std::string_view layer, bool locked) {
  return std::string{layer} + (locked ? " (locked)" : "");
}

void toolbar_apply_see(ToolbarModel& toolbar, const std::vector<std::string>& see_args) {
  static constexpr std::array<std::string_view, 5> kIgnored = {"none", "allSame", "*", "$", "connect"};
  const bool hide = !see_args.empty() && see_args.front() == "no";
  for (std::size_t i = hide ? 1 : 0; i < see_args.size(); ++i) {
    for (const std::string& layer : split_layers(see_args[i])) {
      if (std::find(kIgnored.begin(), kIgnored.end(), layer) != kIgnored.end()) {
        continue;
      }
      std::erase(toolbar.hidden, layer);
      if (hide) {
        toolbar.hidden.push_back(layer);
      }
    }
  }
}

}  // namespace hefesto::gui
