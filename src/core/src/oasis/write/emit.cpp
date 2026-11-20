// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The emit phase of OASIS write (see hefesto/oasis/write/emit.hpp;
// oasis-write "GDS-consistent content", "Write result and error
// propagation", "Determinism"). The GDS element stream is gds-write's own:
// gds::emit_write runs on a copy of the PreparedWrite whose file system
// captures the stream in memory and passes every other access (vendor GDS
// and OASIS files) through, so the structures, names, generation, labels,
// references, vendor copies and full dumps are exactly gds-write's. The
// stream is then translated into an OASIS file, which is written to a
// temporary file next to the target and renamed onto it; a cancelled
// write leaves no file.
// Owner: OASW.

#include "hefesto/oasis/write/emit.hpp"

#include <map>
#include <mutex>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/mask/gen/throttle.hpp"
#include "hefesto/oasis/write/vendor_cells.hpp"
#include "internal.hpp"

namespace hefesto::oasis {

namespace {

namespace codes = detail::codes;

/// Where the GDS stream is captured (never a real path).
const platform::Path kCaptureDirectory{"/.hefesto-oasis-capture"};
const platform::Path kCapturePath = kCaptureDirectory / "stream.gds";

/// Holds the files written under kCaptureDirectory in memory and passes
/// every other access to `base`.
class CaptureFileSystem final : public platform::FileSystem {
 public:
  explicit CaptureFileSystem(platform::FileSystem& base) : base_(base) {}

  diag::Outcome<std::string> read(const platform::Path& path) override {
    if (captured(path)) {
      return memory_.read(path);
    }
    return base_.read(path);
  }
  diag::Status write_atomic(const platform::Path& path, std::string_view data) override {
    return captured(path) ? memory_.write_atomic(path, data) : base_.write_atomic(path, data);
  }
  diag::Status append(const platform::Path& path, std::string_view data) override {
    return captured(path) ? memory_.append(path, data) : base_.append(path, data);
  }
  bool exists(const platform::Path& path) override {
    return captured(path) ? memory_.exists(path) : base_.exists(path);
  }
  diag::Outcome<std::uint64_t> size(const platform::Path& path) override {
    return captured(path) ? memory_.size(path) : base_.size(path);
  }
  diag::Outcome<std::vector<platform::Path>> list(const platform::Path& directory) override {
    return captured(directory) ? memory_.list(directory) : base_.list(directory);
  }
  diag::Status create_directories(const platform::Path& directory) override {
    return captured(directory) ? memory_.create_directories(directory) : base_.create_directories(directory);
  }
  diag::Status rename(const platform::Path& from, const platform::Path& to) override {
    return captured(from) ? memory_.rename(from, to) : base_.rename(from, to);
  }
  diag::Status remove(const platform::Path& path) override {
    return captured(path) ? memory_.remove(path) : base_.remove(path);
  }

  /// The captured stream (empty when none was written), released.
  std::vector<std::byte> take_stream() {
    diag::Outcome<std::string> text = memory_.read(kCapturePath);
    if (!text.ok()) {
      return {};
    }
    std::vector<std::byte> bytes(text.value().size());
    std::copy_n(reinterpret_cast<const std::byte*>(text.value().data()), bytes.size(), bytes.begin());
    (void)memory_.remove(kCapturePath);
    return bytes;
  }

 private:
  static bool captured(const platform::Path& path) {
    return path == kCaptureDirectory || path.parent_path() == kCaptureDirectory;
  }

  platform::FileSystem& base_;
  platform::MemoryFileSystem memory_;
};

bool is_gds_copy(const db::CellDef& def) {
  const db::PropertyStore& p = def.properties();
  const db::PropertyValue* file = p.find("GDS_FILE");
  return file != nullptr && std::holds_alternative<std::string>(*file) && p.find("GDS_START") != nullptr;
}

std::string name_key(const std::vector<std::byte>& payload) {
  std::string text = detail::payload_text(payload);
  return text.substr(0, text.find('\0'));
}

// The structures gds-write generates from the layout, and the label-layer
// texts (LibraryContext).
void classify(const gds::PreparedWrite& prepared, detail::LibraryContext& context) {
  const mask::OutputStyle& style = *prepared.style;
  const gds::CharTable table = gds::char_table(style, prepared.compat);
  const gds::StringOptions strings = gds::string_options(style, prepared.options.settings);
  prepared.editor->cells.for_each([&](db::CellDef& def) {
    if (!is_gds_copy(def) && !is_oasis_vendor_cell(def)) {
      context.generated.insert(name_key(gds::encode_string(def.name(), table, strings, prepared.compat).data));
    }
  });
  for (const mask::OutputLayer& layer : style.layers) {
    if (!layer.label_layer || !layer.gds) {
      continue;
    }
    const std::string mapped = name_key(gds::encode_string(layer.name, table, strings, prepared.compat).data);
    context.label_layer_texts.emplace(layer.gds->layer, layer.gds->datatype, mapped);
    context.label_layer_texts.emplace(layer.gds->layer, layer.gds->datatype, layer.name);
  }
}

EncoderOptions encoder_options(const OasisSettings& settings, const mask::OutputStyle& style) {
  EncoderOptions o;
  o.strict = settings.strict;
  o.validation = settings.validation;
  o.compress = settings.compress;
  o.unit = style.has_option(mask::style_option::angstroms) ? 10000.0 : 1000.0;
  return o;
}

/// The GDS element stream of `prepared` in memory, and the result of the
/// GDS emit (its diagnostics and status).
struct Stream {
  std::vector<std::byte> bytes;
  cmd::CommandResult result = cmd::CommandResult::success();
};

Stream gds_stream(const gds::PreparedWrite& prepared, platform::FileSystem& fs, VendorCells& vendor,
                  const mask::Progress* progress) {
  CaptureFileSystem capture(fs);
  gds::PreparedWrite copy = prepared;
  copy.options.files = &capture;
  copy.options.settings.compress = 0;
  Stream out;
  {
    const VendorCells::Scope scope(vendor);
    out.result = gds::emit_write(copy, kCapturePath, progress);
  }
  out.bytes = capture.take_stream();
  return out;
}

bool cancelled(const cmd::CommandResult& result) {
  for (const diag::Diagnostic& d : result.diagnostics) {
    if (d.code == "GDS-WRITE-013") {
      return true;
    }
  }
  return false;
}

cmd::CommandResult cancelled_result(std::vector<diag::Diagnostic> diagnostics) {
  std::erase_if(diagnostics, [](const diag::Diagnostic& d) { return d.code == "GDS-WRITE-013"; });
  cmd::CommandResult result = cmd::CommandResult::failure(diag::make(codes::kCancelled, "OASIS write cancelled."));
  diagnostics.insert(diagnostics.end(), result.diagnostics.begin(), result.diagnostics.end());
  result.diagnostics = std::move(diagnostics);
  return result;
}

diag::Status save(platform::FileSystem& fs, const platform::Path& path, const std::vector<std::byte>& bytes) {
  mask::OutputFile file(fs, path);
  const std::string_view data{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
  if (diag::Status s = fs.write_atomic(file.temporary(), data); !s.ok()) {
    return diag::make(codes::kOutputFile, "Cannot open " + path.generic_string() + " to write OASIS output");
  }
  if (diag::Status s = file.commit(); !s.ok()) {
    return diag::make(codes::kOutputFile, "Cannot open " + path.generic_string() + " to write OASIS output");
  }
  return diag::Done{};
}

void font_label_warning(std::uint64_t count, std::vector<diag::Diagnostic>& out) {
  if (count > 0) {
    out.push_back(diag::make_native(codes::kFontLabels,
                                    "Warning: " + std::to_string(count) +
                                        " font label(s) written to OASIS without font, size, rotation or "
                                        "justification (OASIS TEXT has no presentation)."));
  }
}

}  // namespace

cmd::CommandResult emit(const gds::PreparedWrite& prepared, const OasisSettings& settings,
                        const platform::Path& path, const mask::Progress* progress) {
  platform::LocalFileSystem local;
  platform::FileSystem& fs = prepared.options.files != nullptr ? *prepared.options.files : local;
  diag::CollectingSink sink;
  VendorCells vendor(prepared, settings, sink);
  Stream stream = gds_stream(prepared, fs, vendor, progress);
  if (cancelled(stream.result) || (progress != nullptr && progress->cancelled())) {
    return cancelled_result(std::move(stream.result.diagnostics));
  }
  detail::LibraryContext context;
  context.settings = &settings;
  context.style = prepared.style.get();
  context.compat = prepared.compat;
  context.sink = &sink;
  classify(prepared, context);
  context.reencode = [&vendor](std::string_view payload, const std::string& output, Encoder& out,
                               detail::OutputNames& names) {
    return vendor.reencode(payload, output, out, [&names](std::string_view p) { return names.of(p); });
  };
  gds::MemorySource source(std::move(stream.bytes));
  gds::MemorySink oasis_bytes;
  Encoder encoder(oasis_bytes, encoder_options(settings, *prepared.style));
  const detail::LibraryResult translated = detail::translate_library(source, context, encoder);
  cmd::CommandResult result = std::move(stream.result);
  std::vector<diag::Diagnostic> mine = sink.take();
  result.diagnostics.insert(result.diagnostics.end(), mine.begin(), mine.end());
  font_label_warning(translated.font_labels, result.diagnostics);
  if (diag::Status s = save(fs, path, oasis_bytes.bytes()); !s.ok()) {
    result.diagnostics.push_back(s.error());
    result.status = cmd::Status::failed;
    return result;
  }
  if (!translated.status.ok()) {
    result.diagnostics.push_back(translated.status.error());
  }
  if (translated.failed || !translated.status.ok()) {
    result.status = cmd::Status::failed;
  }
  return result;
}

}  // namespace hefesto::oasis
