// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Helpers of the OASIS codec tests (tasks.md 10.1-10.5): hand-assembled
// files (magic, START, records, an END record padded to 256 bytes with its
// signature) and a decode-everything driver. Owner: OASFMT.

#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/oasis/codec/decoder.hpp"
#include "hefesto/oasis/codec/primitives.hpp"
#include "hefesto/oasis/codec/validate.hpp"

namespace oasis_test {

namespace oasis = hefesto::oasis;
namespace gds = hefesto::gds;
namespace diag = hefesto::diag;

/// Bytes from hex pairs ("25 53 0D"); other characters are ignored.
inline std::vector<std::byte> hex(std::string_view text) {
  std::vector<std::byte> out;
  int high = -1;
  for (const char c : text) {
    int v = -1;
    if (c >= '0' && c <= '9') {
      v = c - '0';
    } else if (c >= 'A' && c <= 'F') {
      v = c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
      v = c - 'a' + 10;
    }
    if (v < 0) {
      continue;
    }
    if (high < 0) {
      high = v;
    } else {
      out.push_back(static_cast<std::byte>((high << 4) | v));
      high = -1;
    }
  }
  return out;
}

/// The bytes of an n-string, a-string or b-string: length then text.
inline std::vector<std::byte> str(std::string_view text) {
  std::vector<std::byte> out;
  oasis::write_string(out, text);
  return out;
}

inline const std::vector<std::byte>& magic() {
  static const std::vector<std::byte> bytes = hex("25 53 45 4D 49 2D 4F 41 53 49 53 0D 0A");
  return bytes;
}

/// A file under construction.
class File {
 public:
  /// Magic and START "1.0", unit 1000; offset-flag 1 unless `tables` are
  /// given (they are then in START: 12 bytes).
  explicit File(bool offsets_at_end = true) : offsets_at_end_(offsets_at_end) {
    add(magic());
    add(hex("01 03 31 2E 30 00 E8 07"));
    add(hex(offsets_at_end ? "01" : "00 00 00 00 00 00 00 00 00 00 00 00 00"));
  }

  File& add(const std::vector<std::byte>& bytes) {
    bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    return *this;
  }
  File& add(std::string_view hex_text) { return add(hex(hex_text)); }

  [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }

  /// The END record (256 bytes) with `scheme`, the table entries `tables`
  /// (hex, used only when the offsets are at END; default all "00 00").
  [[nodiscard]] std::vector<std::byte> end(oasis::ValidationScheme scheme = oasis::ValidationScheme::crc32,
                                           std::string_view tables = "00 00 00 00 00 00 00 00 00 00 00 00") const {
    std::vector<std::byte> out = bytes_;
    out.push_back(std::byte{2});
    if (offsets_at_end_) {
      const auto t = hex(tables);
      out.insert(out.end(), t.begin(), t.end());
    }
    const std::size_t sig = scheme == oasis::ValidationScheme::none ? 0 : 4;
    const std::size_t end_start = bytes_.size();
    const std::size_t padding = 256 - ((out.size() - end_start) + 1 + sig);
    oasis::write_unsigned(out, padding - 2);
    out.insert(out.end(), padding - 2, std::byte{0});
    out.push_back(static_cast<std::byte>(scheme));
    if (sig != 0) {
      const std::uint32_t value = oasis::signature(scheme, out);
      for (unsigned i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::byte>((value >> (8U * i)) & 0xffU));
      }
    }
    return out;
  }

 private:
  std::vector<std::byte> bytes_;
  bool offsets_at_end_;
};

/// Everything a decoder produced for one file.
struct Decoded {
  std::vector<oasis::Record> records;
  std::vector<diag::Diagnostic> diagnostics;
  std::optional<diag::Diagnostic> error;
  oasis::NameTables names;

  /// The elements in order (dropped ones skipped).
  [[nodiscard]] std::vector<oasis::Element> elements() const {
    std::vector<oasis::Element> out;
    for (const auto& r : records) {
      if (r.element) {
        out.push_back(*r.element);
      }
    }
    return out;
  }

  [[nodiscard]] bool has(std::string_view code) const {
    for (const auto& d : diagnostics) {
      if (d.code == code) {
        return true;
      }
    }
    return error && error->code == code;
  }

  [[nodiscard]] std::string text(std::string_view code) const {
    for (const auto& d : diagnostics) {
      if (d.code == code) {
        return d.text;
      }
    }
    return error && error->code == code ? error->text : std::string{};
  }
};

inline Decoded decode(std::vector<std::byte> bytes, oasis::DecoderOptions options = {}) {
  gds::MemorySource source{std::move(bytes)};
  oasis::Decoder decoder{source, options};
  Decoded out;
  for (;;) {
    auto r = decoder.next();
    if (!r.ok()) {
      out.error = r.error();
      break;
    }
    if (!r.value()) {
      break;
    }
    out.records.push_back(std::move(*r.value()));
  }
  out.diagnostics = decoder.diagnostics();
  out.names = decoder.names();
  return out;
}

inline oasis::Point64 pt(std::int64_t x, std::int64_t y) {
  return {x, y};
}

/// The bytes of tests/data/oasis/codec/<name>.
inline std::vector<std::byte> decode_file(std::string_view name) {
  std::ifstream in{std::string{HEFESTO_TEST_DATA_DIR} + "/" + std::string{name}, std::ios::binary};
  const std::vector<char> chars{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
  std::vector<std::byte> out;
  for (const char c : chars) {
    out.push_back(static_cast<std::byte>(c));
  }
  return out;
}

}  // namespace oasis_test
