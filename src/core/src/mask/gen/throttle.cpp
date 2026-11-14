// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Progress throttling and the temporary output file (Q-MASK-414; design
// D8). The reporting rule follows Magic VLSI 8.3.694 (fc5daa9e)
// cif/CIFhier.c CIFGenSubcells (:1035-1041, :1070-1100, :1110-1124),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: GEN.

#include "hefesto/mask/gen/throttle.hpp"

#include <utility>

namespace hefesto::mask {

ProgressThrottle::ProgressThrottle(const Progress* progress, platform::Clock* clock, std::chrono::nanoseconds interval)
    : progress_(progress), clock_(clock), interval_(interval) {
  deadline_ = now() + interval_;
}

std::chrono::steady_clock::time_point ProgressThrottle::now() const {
  return clock_ != nullptr ? clock_->monotonic_now() : std::chrono::steady_clock::now();
}

void ProgressThrottle::step(std::uint64_t done, std::uint64_t total) {
  if (progress_ == nullptr || total == 0 || done <= 1) {
    return;
  }
  const double pdone = 100.0 * (static_cast<double>(done) / static_cast<double>(total));
  if (pdone - last_ <= 1.0 && done != total) {
    return;
  }
  const std::chrono::steady_clock::time_point t = now();
  if (t < deadline_) {
    return;  // the five-second timer has not expired
  }
  reported_ = true;
  last_ = pdone;
  progress_->update(pdone / 100.0);
  deadline_ = t + interval_;
}

void ProgressThrottle::finish() {
  if (progress_ != nullptr && reported_) {
    progress_->update(1.0);
  }
}

OutputFile::OutputFile(platform::FileSystem& fs, platform::Path target) : fs_(fs), target_(std::move(target)) {
  temporary_ = target_.parent_path() / ("." + target_.filename().string() + ".hefesto-tmp");
}

OutputFile::~OutputFile() {
  if (!done_) {
    discard();
  }
}

diag::Status OutputFile::commit() {
  done_ = true;
  return fs_.rename(temporary_, target_);
}

void OutputFile::discard() {
  done_ = true;
  (void)fs_.remove(temporary_);
}

}  // namespace hefesto::mask
