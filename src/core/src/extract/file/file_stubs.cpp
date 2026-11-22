// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs of extract/file (writer.hpp, atomic_output.hpp): neutral
// values or CORE-NOTIMPL-008 until EXTNODE lands them
// (openspec/changes/m7-extraction tasks.md 5.5). EXTNODE replaces this file
// with its own sources.
//
// Owner: EXTNODE (design D3).

#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/extract/file/atomic_output.hpp"
#include "hefesto/extract/file/writer.hpp"

namespace hefesto::extract {

std::string format_value(double /*value*/) {
  return {};
}

void ExtWriter::header(const ExtHeader& /*header*/) {}
void ExtWriter::node(std::string_view /*name*/, std::int64_t /*resistance*/, double /*cap*/, db::Point /*at*/,
                     std::string_view /*type*/, std::span<const std::int64_t> /*area_perimeter*/) {}
void ExtWriter::attr(std::string_view /*node*/, db::Rect /*at*/, std::string_view /*type*/,
                     std::string_view /*text*/) {}
void ExtWriter::equiv(std::string_view /*a*/, std::string_view /*b*/) {}
void ExtWriter::port(std::string_view /*name*/, std::int32_t /*index*/, db::Rect /*at*/, std::string_view /*type*/) {}
void ExtWriter::substrate(std::string_view /*name*/, std::int64_t /*resistance*/, double /*cap*/) {}
void ExtWriter::device(std::string_view /*device_class*/, std::string_view /*model*/, db::Rect /*at*/,
                       std::span<const std::string> /*params*/, std::string_view /*substrate*/,
                       std::span<const ExtTerminal> /*terminals*/) {}
void ExtWriter::parameters(std::string_view /*model*/, std::span<const std::string> /*params*/) {}
void ExtWriter::cap(std::string_view /*a*/, std::string_view /*b*/, double /*value*/) {}
void ExtWriter::resist(std::string_view /*a*/, std::string_view /*b*/, double /*value*/) {}
void ExtWriter::use(std::string_view /*def*/, std::string_view /*id*/, std::span<const std::int32_t> /*transform*/) {}
void ExtWriter::merge(std::string_view /*a*/, std::string_view /*b*/, double /*cap*/,
                      std::span<const std::int64_t> /*area_perimeter*/) {}
void ExtWriter::killnode(std::string_view /*name*/) {}
void ExtWriter::distance(std::string_view /*driver*/, std::string_view /*receiver*/, std::int64_t /*min*/,
                         std::int64_t /*max*/) {}

diag::Outcome<std::string> ExtWriter::finish() {
  lines_.clear();
  return cmd::m7_not_implemented_diagnostic("extract writer");
}

diag::Outcome<AtomicOutput> AtomicOutput::open(const cell::LookupContext& /*ctx*/, const db::CellDef& /*cell*/,
                                               std::string_view suffix) {
  return cmd::m7_not_implemented_diagnostic("extract output " + std::string{suffix});
}

diag::Status AtomicOutput::write(std::string_view /*text*/) {
  return cmd::m7_not_implemented_status("extract output write");
}

diag::Status AtomicOutput::commit() {
  return cmd::m7_not_implemented_status("extract output commit");
}

void AtomicOutput::abandon() noexcept {
  temporary_.reset();
}

}  // namespace hefesto::extract
