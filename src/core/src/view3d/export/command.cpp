// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `export3d` (hefesto/view3d/export/command.hpp; design D14, task 11.3).
//
// Owner: EXPORT (design D3).

#include "hefesto/view3d/export/command.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/view3d/client/scene.hpp"
#include "hefesto/view3d/export/export.hpp"
#include "view3d/export/model.hpp"

namespace hefesto::view3d {

namespace {

constexpr std::string_view kNoWindow = "VIEW3D-EXPORT-001";
constexpr std::string_view kUsage = "VIEW3D-EXPORT-002";
constexpr std::string_view kNoModel = "VIEW3D-EXPORT-004";
constexpr std::string_view kUsageText =
    "Usage: export3d file [-format gltf|glb|stl] [-merged] [-zscale k] [-visible|-all]";

struct Request {
  std::string file;
  std::optional<ExportFormat> format;
  bool merged = false;
  std::optional<double> zscale;
  bool visible_only = true;
};

std::string lower(std::string_view word) {
  std::string out(word);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

std::optional<ExportFormat> format_named(std::string_view word) {
  const std::string name = lower(word);
  if (name == "gltf") {
    return ExportFormat::gltf;
  }
  if (name == "glb") {
    return ExportFormat::glb;
  }
  if (name == "stl") {
    return ExportFormat::stl;
  }
  return std::nullopt;
}

std::optional<double> positive_number(const std::string& word) {
  char* end = nullptr;
  errno = 0;
  const double value = std::strtod(word.c_str(), &end);
  if (end == word.c_str() || *end != '\0' || errno != 0 || !std::isfinite(value) || value <= 0.0) {
    return std::nullopt;
  }
  return value;
}

// The words an option takes (with its value), or nullopt when it is not one.
std::optional<std::size_t> take_option(const std::vector<std::string>& args, std::size_t i, Request& r) {
  const std::string& word = args[i];
  const bool has_value = i + 1 < args.size();
  if (word == "-merged") {
    r.merged = true;
    return 1;
  }
  if (word == "-visible" || word == "-all") {
    r.visible_only = word == "-visible";
    return 1;
  }
  if (word == "-format" && has_value) {
    r.format = format_named(args[i + 1]);
    return r.format ? std::optional<std::size_t>{2} : std::nullopt;
  }
  if (word == "-zscale" && has_value) {
    r.zscale = positive_number(args[i + 1]);
    return r.zscale ? std::optional<std::size_t>{2} : std::nullopt;
  }
  return std::nullopt;
}

diag::Outcome<Request> parse(const std::vector<std::string>& args) {
  Request r;
  for (std::size_t i = 0; i < args.size();) {
    if (!args[i].starts_with('-')) {
      if (!r.file.empty()) {
        return diag::make(kUsage, std::string{kUsageText});
      }
      r.file = args[i++];
      continue;
    }
    const std::optional<std::size_t> used = take_option(args, i, r);
    if (!used) {
      return diag::make(kUsage, std::string{kUsageText});
    }
    i += *used;
  }
  if (r.file.empty()) {
    return diag::make(kUsage, std::string{kUsageText});
  }
  if (!r.format) {
    const std::string extension = platform::Path{r.file}.extension().string();
    r.format = format_named(extension.empty() ? extension : extension.substr(1));
  }
  if (!r.format) {
    return diag::make(kUsage, "export3d: cannot tell the format of \"" + r.file +
                                  "\"; give -format gltf|glb|stl or a .gltf, .glb or .stl file");
  }
  return r;
}

// A relative URI reference for a file name (RFC 3986 unreserved characters
// kept, everything else percent-encoded).
std::string uri_of(std::string_view name) {
  static constexpr std::string_view kHex = "0123456789ABCDEF";
  std::string out;
  for (const char c : name) {
    const auto byte = static_cast<unsigned char>(c);
    if (std::isalnum(byte) != 0 || c == '-' || c == '.' || c == '_' || c == '~') {
      out.push_back(c);
    } else {
      out += {'%', kHex[byte >> 4U], kHex[byte & 0xFU]};
    }
  }
  return out;
}

class Exporter {
 public:
  Exporter(const Scene& scene, ExportOptions options, platform::FileSystem& fs, diag::DiagnosticSink& sink)
      : scene_(scene), options_(std::move(options)), fs_(fs), sink_(sink) {}

  diag::Status run(const platform::Path& path) {
    if (options_.format != ExportFormat::stl) {
      return gltf(path);
    }
    return options_.merged ? stl(path, sink_) : stl_per_layer(path);
  }

  [[nodiscard]] cmd::Value::List written() const { return written_; }

 private:
  diag::Status save(const platform::Path& path, const std::vector<std::byte>& bytes) {
    const std::string_view data{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    diag::Status status = fs_.write_atomic(path, data);
    if (status.ok()) {
      written_.emplace_back(path.generic_string());
    }
    return status;
  }

  diag::Status gltf(const platform::Path& path) {
    gds::MemorySink main;
    gds::MemorySink bin;
    const std::string bin_name = path.stem().string() + ".bin";
    if (options_.format == ExportFormat::gltf) {
      options_.bin_out = &bin;
      options_.bin_uri = uri_of(bin_name);
    }
    if (diag::Status s = write_gltf(*scene_.mesh, scene_.stack, options_, main, sink_); !s.ok()) {
      return s;
    }
    if (!bin.bytes().empty()) {
      if (diag::Status s = save(path.parent_path() / bin_name, bin.bytes()); !s.ok()) {
        return s;
      }
    }
    return save(path, main.bytes());
  }

  diag::Status stl(const platform::Path& path, diag::DiagnosticSink& sink) {
    gds::MemorySink out;
    if (diag::Status s = write_stl(*scene_.mesh, scene_.stack, options_, out, sink); !s.ok()) {
      return s;
    }
    return save(path, out.bytes());
  }

  // One file per layer; the layers' warnings are emitted once.
  diag::Status stl_per_layer(const platform::Path& path) {
    diag::NullSink quiet;
    const std::string stem = path.stem().string();
    for (const ExportLayer& layer : export_layers(*scene_.mesh, scene_.stack, options_, sink_)) {
      options_.stl_layer = layer.layer;
      const platform::Path file = path.parent_path() / (stem + "_" + detail::safe_file_part(layer.name) + ".stl");
      if (diag::Status s = stl(file, quiet); !s.ok()) {
        return s;
      }
    }
    return diag::Done{};
  }

  const Scene& scene_;
  ExportOptions options_;
  platform::FileSystem& fs_;
  diag::DiagnosticSink& sink_;
  cmd::Value::List written_;
};

ExportOptions options_for(const editor::EditorState& editor, const Scene& scene, const Request& r) {
  ExportOptions options;
  options.format = *r.format;
  options.merged = r.merged;
  options.zscale = r.zscale;
  options.visible_only = r.visible_only;
  options.styles = editor.styles ? &*editor.styles : nullptr;
  options.hidden = scene.hidden;
  if (scene.stack.mode() == Mode::mask_layer) {
    options.output_style = editor.mask.output_styles.current_style();
  }
  return options;
}

cmd::CommandResult export3d_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  diag::Outcome<Request> request = parse(inv.args);
  if (!request.ok()) {
    return cmd::CommandResult::failure(request.error());
  }
  diag::Outcome<editor::EditorState*> editor = cmd::require_editor(ctx);
  if (!editor.ok()) {
    return cmd::CommandResult::failure(editor.error());
  }
  const editor::EditorState& state = *editor.value();
  if (!state.view3d.window) {
    return cmd::CommandResult::failure(diag::make(kNoWindow, "export3d: no 3D window is open"));
  }
  BuildContext build;
  const Scene scene = build_scene(state, *state.view3d.window, ctx.project().compat.magic_compat, build);
  if (!scene.mesh) {
    return cmd::CommandResult::failure(diag::make(
        kNoModel, "export3d: the 3D window has no model (cell \"" + state.view3d.window->cell + "\" not found)"));
  }
  Exporter exporter(scene, options_for(state, scene, request.value()), ctx.services().files, ctx.sink());
  if (diag::Status s = exporter.run(platform::Path{request.value().file}); !s.ok()) {
    return cmd::CommandResult::failure(s.error());
  }
  return cmd::CommandResult::success(exporter.written());
}

}  // namespace

bool register_export3d_command(cmd::CommandRegistry& registry) {
  return !registry
              .add(cmd::CommandSpec{"export3d", std::string{kUsageText.substr(7)},
                                    "Exports the 3D window's model as glTF, GLB or binary STL in microns.",
                                    export3d_cmd})
              .has_value();
}

}  // namespace hefesto::view3d
