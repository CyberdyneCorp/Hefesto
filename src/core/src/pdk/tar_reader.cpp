// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Safe tar unpacking (POSIX.1-2001 ustar/pax, GNU long names).

#include "hefesto/pdk/tar_reader.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <system_error>

#include "hefesto/pdk/diag_codes.hpp"

namespace hefesto::pdk {

namespace {

constexpr std::size_t kBlock = 512;
using Block = std::array<char, kBlock>;

diag::Diagnostic bad_archive(std::string text) {
  return diag::make(codes::kBadArchive, std::move(text));
}

std::string_view field(const Block& block, std::size_t offset, std::size_t length) {
  const std::string_view raw{block.data() + offset, length};
  const auto nul = raw.find('\0');
  return nul == std::string_view::npos ? raw : raw.substr(0, nul);
}

// Octal numeric field, or GNU base-256 when the high bit of the first byte is set.
std::optional<std::uint64_t> number(const Block& block, std::size_t offset, std::size_t length) {
  const auto first = static_cast<unsigned char>(block[offset]);
  std::uint64_t value = 0;
  if ((first & 0x80U) != 0) {
    for (std::size_t i = 1; i < length; ++i) {
      if (value > (UINT64_MAX >> 8)) {
        return std::nullopt;
      }
      value = (value << 8) | static_cast<unsigned char>(block[offset + i]);
    }
    return value;
  }
  bool digits = false;
  for (std::size_t i = 0; i < length; ++i) {
    const char c = block[offset + i];
    if (c == ' ' && !digits) {
      continue;
    }
    if (c < '0' || c > '7') {
      break;
    }
    if (value > (UINT64_MAX >> 3)) {
      return std::nullopt;
    }
    value = value * 8 + static_cast<std::uint64_t>(c - '0');
    digits = true;
  }
  return value;
}

bool checksum_ok(const Block& block) {
  const auto stored = number(block, 148, 8);
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < kBlock; ++i) {
    sum += (i >= 148 && i < 156) ? std::uint64_t{' '} : static_cast<unsigned char>(block[i]);
  }
  return stored && *stored == sum;
}

bool all_zero(const Block& block) {
  for (const char c : block) {
    if (c != '\0') {
      return false;
    }
  }
  return true;
}

std::uint64_t padded(std::uint64_t size) {
  return (size + kBlock - 1) / kBlock * kBlock;
}

// The decimal length that starts a pax record; std::nullopt when malformed.
std::optional<std::size_t> pax_length(std::string_view digits) {
  std::size_t length = 0;
  for (const char c : digits) {
    if (c < '0' || c > '9') {
      return std::nullopt;
    }
    length = length * 10 + static_cast<std::size_t>(c - '0');
  }
  return length;
}

// The `path` record of a pax extended header ("<len> <key>=<value>\n"...).
std::optional<std::string> pax_path(std::string_view data) {
  std::optional<std::string> path;
  while (!data.empty()) {
    const auto space = data.find(' ');
    const auto length = space == std::string_view::npos ? std::nullopt : pax_length(data.substr(0, space));
    if (!length || *length <= space + 1 || *length > data.size()) {
      break;
    }
    std::string_view record = data.substr(space + 1, *length - space - 1);
    if (record.ends_with('\n')) {
      record.remove_suffix(1);
    }
    if (record.starts_with("path=")) {
      path = std::string{record.substr(5)};
    }
    data.remove_prefix(*length);
  }
  return path;
}

class TarUnpacker {
 public:
  TarUnpacker(std::istream& in, const platform::Path& root, std::uint64_t max_bytes, const EntryFilter& filter)
      : in_(in), root_(root), max_bytes_(max_bytes), filter_(filter) {}

  diag::Outcome<UnpackReport> run() {
    Block block{};
    while (read_block(block)) {
      if (all_zero(block)) {
        return report_;
      }
      if (!checksum_ok(block)) {
        return bad_archive("tar header checksum mismatch at entry " + std::to_string(entries_ + 1));
      }
      ++entries_;
      if (auto status = entry(block); !status) {
        return status.error();
      }
    }
    if (entries_ == 0) {
      return bad_archive("the archive is empty or not a tar archive");
    }
    return report_;  // end of stream without the zero blocks: accepted, like tar(1)
  }

 private:
  bool read_block(Block& block) {
    in_.read(block.data(), kBlock);
    return in_.gcount() == static_cast<std::streamsize>(kBlock);
  }

  diag::Outcome<std::string> read_data(std::uint64_t size) {
    if (size > (std::uint64_t{1} << 20)) {
      return bad_archive("tar metadata entry is too large");
    }
    const std::size_t padded_size = padded(size);
    std::string data(padded_size, '\0');
    in_.read(data.data(), static_cast<std::streamsize>(data.size()));
    if (in_.gcount() != static_cast<std::streamsize>(data.size())) {
      return bad_archive("tar archive is truncated");
    }
    const std::size_t data_size = size;
    data.resize(data_size);
    return data;
  }

  // Skips an entry's data and its padding to the next block.
  diag::Status skip(std::uint64_t size) { return skip_bytes(padded(size)); }

  diag::Status skip_bytes(std::uint64_t count) {
    std::array<char, 65536> buffer{};
    std::uint64_t left = count;
    while (left > 0) {
      const std::uint64_t take = std::min<std::uint64_t>(left, buffer.size());
      in_.read(buffer.data(), static_cast<std::streamsize>(take));
      if (in_.gcount() != static_cast<std::streamsize>(take)) {
        return bad_archive("tar archive is truncated");
      }
      left -= take;
    }
    return diag::Done{};
  }

  std::string header_name(const Block& block) const {
    if (pending_name_) {
      return *pending_name_;
    }
    std::string name{field(block, 0, 100)};
    if (field(block, 257, 5) == "ustar") {
      if (const std::string_view prefix = field(block, 345, 155); !prefix.empty()) {
        name = std::string{prefix} + "/" + name;
      }
    }
    return name;
  }

  diag::Status entry(const Block& block) {
    const auto size = number(block, 124, 12);
    if (!size) {
      return bad_archive("tar entry size is not a number");
    }
    const char type = block[156];
    if (type == 'x' || type == 'L') {
      return metadata(type, *size);
    }
    if (type == 'g' || type == 'K') {
      return skip(*size);  // global pax header, GNU long link name: nothing to create
    }
    const std::string name = header_name(block);
    pending_name_.reset();
    if (type == '0' || type == '\0' || type == '7') {
      return regular_file(name, *size);
    }
    if (type != '5') {
      report_.skipped.push_back(name);  // links, devices, FIFOs are never created
    } else if (auto path = safe_entry_path(root_, name); !path) {
      return path.error();  // even skipped directory names must be safe
    }
    return skip(*size);
  }

  diag::Status metadata(char type, std::uint64_t size) {
    auto data = read_data(size);
    if (!data) {
      return data.error();
    }
    if (type == 'L') {
      const std::string& text = data.value();
      pending_name_ = text.substr(0, text.find('\0'));
    } else if (auto path = pax_path(data.value())) {
      pending_name_ = std::move(*path);
    }
    return diag::Done{};
  }

  diag::Status regular_file(const std::string& name, std::uint64_t size) {
    auto path = safe_entry_path(root_, name);
    if (!path) {
      return path.error();
    }
    if (path.value().empty() || !accepted(name)) {
      return skip(size);
    }
    if (size > max_bytes_ || report_.bytes > max_bytes_ - size) {
      return diag::make(codes::kUnpackLimit, "unpacked size exceeds the limit of " + std::to_string(max_bytes_) +
                                                 " bytes at entry '" + name + "'");
    }
    if (auto status = write_file(path.value(), size); !status) {
      return status;
    }
    report_.bytes += size;
    ++report_.files;
    return diag::Done{};
  }

  diag::Status write_file(const platform::Path& path, std::uint64_t size) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out{path, std::ios::binary | std::ios::trunc};
    if (ec || !out) {
      return diag::make(codes::kInstallIo, "cannot create " + path.string());
    }
    std::array<char, 65536> buffer{};
    std::uint64_t left = size;
    while (left > 0) {
      const std::uint64_t take = std::min<std::uint64_t>(left, buffer.size());
      in_.read(buffer.data(), static_cast<std::streamsize>(take));
      if (in_.gcount() != static_cast<std::streamsize>(take)) {
        return bad_archive("tar archive is truncated in " + path.filename().string());
      }
      out.write(buffer.data(), static_cast<std::streamsize>(take));
      left -= take;
    }
    if (!out) {
      return diag::make(codes::kInstallIo, "cannot write " + path.string());
    }
    return skip_bytes(padded(size) - size);  // the padding only: skip() would pad again
  }

  bool accepted(std::string_view name) const {
    while (name.starts_with("./")) {
      name.remove_prefix(2);
    }
    return !filter_ || filter_(name);
  }

  std::istream& in_;
  const platform::Path& root_;
  std::uint64_t max_bytes_;
  const EntryFilter& filter_;
  std::optional<std::string> pending_name_;
  std::size_t entries_ = 0;
  UnpackReport report_;
};

}  // namespace

diag::Outcome<platform::Path> safe_entry_path(const platform::Path& root, std::string_view name) {
  const auto unsafe = [&] {
    return diag::make(codes::kUnsafeEntry, "unsafe archive entry '" + std::string{name} + "'");
  };
  if (name.empty() || name.front() == '/' || name.front() == '\\' || name.find(':') != std::string_view::npos ||
      name.find('\\') != std::string_view::npos) {
    return unsafe();
  }
  platform::Path relative;
  std::size_t start = 0;
  while (start <= name.size()) {
    const auto slash = name.find('/', start);
    const auto part = name.substr(start, slash == std::string_view::npos ? std::string_view::npos : slash - start);
    if (part == "..") {
      return unsafe();
    }
    if (!part.empty() && part != ".") {
      relative /= std::string{part};
    }
    if (slash == std::string_view::npos) {
      break;
    }
    start = slash + 1;
  }
  if (relative.empty()) {
    return platform::Path{};
  }
  const platform::Path full = (root / relative).lexically_normal();
  const platform::Path inside = full.lexically_relative(root.lexically_normal());
  if (inside.empty() || *inside.begin() == "..") {
    return unsafe();
  }
  return full;
}

diag::Outcome<UnpackReport> unpack_tar(std::istream& in, const platform::Path& root, std::uint64_t max_bytes,
                                       const EntryFilter& filter) {
  return TarUnpacker{in, root, max_bytes, filter}.run();
}

}  // namespace hefesto::pdk
