// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (m10-plot-route tasks.md 3.1): an AtomicFile removes its
// temporary file unless committed; open_atomic and commit report
// CORE-NOTIMPL-010 and with_default_extension returns the path unchanged
// until PLOTCMD (task 5.3) lands them.
//
// Owner: PLOTCMD.

#include "hefesto/plot/io/atomic_file.hpp"

#include <system_error>
#include <utility>

#include "hefesto/plot/not_implemented.hpp"

namespace hefesto::plot {

AtomicFile::AtomicFile(std::filesystem::path target, std::filesystem::path temporary)
    : target_(std::move(target)), temporary_(std::move(temporary)) {}

AtomicFile::AtomicFile(AtomicFile&& other) noexcept
    : target_(std::move(other.target_)),
      temporary_(std::move(other.temporary_)),
      out_(std::move(other.out_)),
      committed_(other.committed_) {
  other.temporary_.clear();
}

AtomicFile& AtomicFile::operator=(AtomicFile&& other) noexcept {
  if (this != &other) {
    discard();
    target_ = std::move(other.target_);
    temporary_ = std::move(other.temporary_);
    out_ = std::move(other.out_);
    committed_ = other.committed_;
    other.temporary_.clear();
  }
  return *this;
}

AtomicFile::~AtomicFile() { discard(); }

void AtomicFile::discard() noexcept {
  if (out_.is_open()) {
    out_.close();
  }
  if (!committed_ && !temporary_.empty()) {
    std::error_code ignored;
    std::filesystem::remove(temporary_, ignored);
  }
  temporary_.clear();
}

diag::Status AtomicFile::commit() { return not_implemented("plot::AtomicFile::commit"); }

diag::Outcome<AtomicFile> open_atomic(const std::filesystem::path& /*target*/) {
  return not_implemented("plot::open_atomic");
}

std::filesystem::path with_default_extension(const std::filesystem::path& path, std::string_view /*ext*/,
                                             bool /*magic_compat*/) {
  return path;
}

}  // namespace hefesto::plot
