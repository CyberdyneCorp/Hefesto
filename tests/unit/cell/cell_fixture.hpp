// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared fixture of the CELL unit tests: a CellIoContext over an in-memory
// (or local) file system with the tiny test technology or a loaded one.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "hefesto/cell/fonts.hpp"
#include "hefesto/cell/io_safety.hpp"
#include "hefesto/cell/mag_io.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/clock.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"

namespace hefesto::cell::test {

inline std::shared_ptr<const tech::Technology> tiny_technology() {
  static const std::shared_ptr<const tech::Technology> kTech = [] {
    platform::LocalFileSystem local;
    diag::CollectingSink sink;
    auto loaded = tech::load_technology(local, std::string{HEFESTO_SOURCE_DIR} + "/tests/data/tech/tiny.tech",
                                        tech::LoadOptions{}, sink);
    REQUIRE(loaded.ok());
    return loaded.value();
  }();
  return kTech;
}

/// The directory of Magic's default fonts in the reference tree, if present.
inline std::optional<std::filesystem::path> magic_font_dir() {
  const std::filesystem::path dir = std::string{HEFESTO_SOURCE_DIR} + "/build/ref/magic/graphics";
  std::error_code ec;
  return std::filesystem::exists(dir / "FreeSans.pt3", ec) ? std::optional{dir} : std::nullopt;
}

class CellIo {
 public:
  explicit CellIo(std::shared_ptr<const tech::Technology> tech = tiny_technology(),
                  platform::FileSystem* fs = nullptr)
      : tech_(std::move(tech)),
        fs_(fs != nullptr ? fs : &memory_),
        host_(make_memory_file_host(*fs_, "/work")),
        ctx{*fs_, clock, library, tech_.get(), grid, paths, vars, sink, *host_, stamps, locks.get(), &fonts,
            true, true, nullptr, {}, {}} {
    ctx.rescale_database = [](std::int64_t, std::int64_t) { return true; };
  }
  CellIo(const CellIo&) = delete;
  CellIo& operator=(const CellIo&) = delete;

  /// Reads `text` into a new definition `name`.
  db::CellDef& read(const std::string& name, std::string_view text, diag::Status* status = nullptr) {
    auto created = library.create(name, tech_->plane_count());
    REQUIRE(created.ok());
    auto done = read_mag(ctx, *created.value(), text);
    if (status != nullptr) {
      *status = done;
    }
    return *created.value();
  }

  [[nodiscard]] std::string write(const db::CellDef& def) const { return write_mag(ctx, def); }

  /// Loads Magic's default fonts from the reference tree; false when absent.
  bool load_fonts() {
    const auto dir = magic_font_dir();
    if (!dir) {
      return false;
    }
    platform::LocalFileSystem local;
    return load_default_fonts(local, {*dir}, fonts, sink) == 3;
  }

  [[nodiscard]] bool has_code(std::string_view code) const {
    for (const auto& d : sink.snapshot()) {
      if (d.code == code) {
        return true;
      }
    }
    return false;
  }

 private:
  std::shared_ptr<const tech::Technology> tech_;
  platform::MemoryFileSystem memory_;
  platform::FileSystem* fs_;
  std::unique_ptr<FileHost> host_;

 public:
  platform::FixedClock clock{std::chrono::system_clock::time_point{std::chrono::seconds{1000}}};
  db::CellLibrary library;
  tech::GridScale grid;
  SearchPaths paths;
  PathVariables vars;
  diag::CollectingSink sink;
  TimestampLog stamps;
  std::unique_ptr<FileLockService> locks = make_in_process_lock_service();
  display::FontRegistry fonts;
  CellIoContext ctx;
};

}  // namespace hefesto::cell::test
