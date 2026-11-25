// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the LVS SPICE/CDL reader (capability
// lvs-netlist-input; openspec/changes/m9-lvs-simulation design.md D20,
// hefesto-testing-strategy "Parser fuzzing"). The first byte picks the
// options (library read, automatic black boxes, ignore R and C, lowered
// limits, a case-sensitive database); the rest is the netlist, read once in
// memory and once as a file that includes itself, so include cycles, the
// limits and the expression evaluator are exercised. Seeds are
// Hefesto-authored (corpus/fuzz_lvs_spice/).
//
// Owner: LVSREAD.

#include <cstddef>
#include <cstdint>
#include <string>

#include "fuzz_common.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/lvs/net/expression.hpp"
#include "hefesto/lvs/net/si_value.hpp"
#include "hefesto/lvs/read/reader.hpp"
#include "hefesto/platform/file_system.hpp"

namespace {

namespace lvs = hefesto::lvs;

lvs::ReadOptions options_for(std::uint8_t mode, hefesto::diag::DiagnosticSink& sink,
                             hefesto::platform::FileSystem& files) {
  lvs::ReadOptions options;
  options.sink = &sink;
  options.files = &files;
  options.library = (mode & 1U) != 0;
  options.auto_blackbox = (mode & 2U) != 0;
  options.ignore_rc = (mode & 4U) != 0;
  if ((mode & 8U) != 0) {
    options.limits.include_depth = 4;
    options.limits.logical_line = 4096;
    options.limits.name_length = 64;
    options.limits.devices = 1000;
  }
  options.ignore = {{"fill", lvs::IgnoredClass::Kind::class_}, {"short", lvs::IgnoredClass::Kind::shorted}};
  options.home_directory = [](std::string_view user) -> std::optional<std::string> {
    if (!user.empty()) {
      return std::nullopt;
    }
    return std::string{"/home"};
  };
  return options;
}

void evaluate_properties(const lvs::NetlistDb& db) {
  for (const auto& cell : db.cells) {
    const auto parameters = db.parameters.find(cell.file);
    for (const auto& instance : cell.instances) {
      for (const auto& record : instance.properties) {
        const lvs::ParameterScope scope{&record, &db.cells.at(instance.cell),
                                        parameters == db.parameters.end() ? nullptr : &parameters->second};
        for (const auto& property : record) {
          (void)lvs::resolve_value(property.value, scope);
          (void)lvs::is_si_number(property.value);
        }
      }
    }
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  hefesto::diag::NullSink sink;
  hefesto::platform::MemoryFileSystem files;
  const std::string body{text};
  (void)files.write_atomic("/w/in.spice", body);
  (void)files.write_atomic("/w/self.sp", ".include in.spice\n" + body);
  const auto options = options_for(mode, sink, files);

  lvs::NetlistDb memory;
  memory.case_sensitive_read = (mode & 16U) != 0;
  (void)lvs::read_netlist(memory, lvs::Source{"/w/mem.spice", body}, lvs::Format::cdl, options);
  evaluate_properties(memory);

  lvs::NetlistDb file;
  (void)lvs::read_netlist(file, lvs::Source{"/w/self.sp", std::nullopt}, lvs::Format::spice, options);
  (void)lvs::detect_format(lvs::Source{"netlist", body});
  return 0;
}
