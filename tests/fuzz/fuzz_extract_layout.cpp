// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the extractor on hostile layouts (task 19.2,
// m7-extraction design D22, D24 M7-S3; capabilities extract-nodes-and-labels,
// extract-capacitance, extract-devices, extract-hierarchy, extract-commands,
// extresist-network; hefesto-testing-strategy "Parser fuzzing").
//
// The input is decoded into paint and labels of two cells on the synthetic
// technology tests/fuzz/data/exttest_fuzz.tech (the golden cases'
// exttest.tech with a fuller extract section): `sub`, and `top` with
// one or two uses of `sub`. The cells are written as `.mag` files on an
// in-memory file system and run through the commands, as a user runs them:
// `extract all` (extract_cell over the hierarchy), `ext2spice` (the `.ext`
// files read back by EXTFLAT) and, with bit 1, `extresist all`.
//
// The first byte is a mode:
//   bit 0  magic-compat OFF
//   bit 1  `extresist all` after the extraction
//   bit 2  `extract do corners` and `extract no adjust`
//   bit 3  the second use of `sub` is a 1 x 3 array
//   bits 4-5  the offset of the first use (0, 3, 7 or 16 lambda)
// The rest is a list of 5-byte records: a type byte (bits 0-4 the type,
// bit 5 a label on the rectangle, bit 6 the record belongs to `sub`), x and
// y (signed bytes), width and height (1-32).
//
// Resource caps (harness, design D22; recorded as approved under the
// standing rule of 2026-11-25): at most 64 rectangles and 8 labels per
// cell, coordinates within -128..159 lambda, so the step squares, the
// halo interactions and the extresist networks stay small and a time-out
// is a real hang.
//
// Owner: INTEGRATION (tests/fuzz/, design D2).

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "fuzz_common.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/ui/line.hpp"
#include "hefesto/view/window.hpp"

namespace {

constexpr std::uint8_t kCompatOff = 0x01;
constexpr std::uint8_t kExtresist = 0x02;
constexpr std::uint8_t kCorners = 0x04;
constexpr std::uint8_t kArray = 0x08;

constexpr std::size_t kMaxRects = 64;
constexpr std::size_t kMaxLabels = 8;
constexpr std::size_t kRecord = 5;

constexpr std::array<std::string_view, 20> kTypes{
    "pwell", "nwell", "dnwell", "ndiff", "pdiff", "psd",   "ndiffh", "nfet",    "pfet",   "poly",
    "rpoly", "ndc",   "m1c",    "metal1", "via",  "metal2", "metal3", "mimcap", "thkox", "metal1"};
constexpr std::array<std::string_view, 8> kLabels{"a", "b", "vdd!", "gnd!", "in", "out", "a_uq0", "b#"};

/// The `.mag` text of one cell under construction.
struct CellText {
  std::string paint;
  std::string labels;
  std::size_t rects = 0;
  std::size_t label_count = 0;

  void add(std::string_view type, int x, int y, int w, int h, bool label) {
    if (rects == kMaxRects) {
      return;
    }
    ++rects;
    const std::string box =
        std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(x + w) + " " + std::to_string(y + h);
    paint += "<< " + std::string{type} + " >>\nrect " + box + "\n";
    if (label && label_count < kMaxLabels) {
      labels += "rlabel " + std::string{type} + " " + box + " 0 " + std::string{kLabels[label_count]} + "\n";
      ++label_count;
    }
  }

  [[nodiscard]] std::string mag(std::string_view uses) const {
    return "magic\ntech exttest\ntimestamp 100\n" + paint + "<< labels >>\n" + labels + std::string{uses} +
           "<< end >>\n";
  }
};

std::string use_text(std::string_view id, int x, bool array) {
  std::string text = "use sub " + std::string{id} + "\n";
  if (array) {
    text += "array 0 2 40 0 0 0\n";
  }
  return text + "timestamp 100\ntransform 1 0 " + std::to_string(x) + " 0 1 0\nbox 0 0 1 1\n";
}

void decode(std::string_view bytes, CellText& top, CellText& sub) {
  for (std::size_t at = 0; at + kRecord <= bytes.size(); at += kRecord) {
    const auto b = [&](std::size_t i) { return static_cast<std::uint8_t>(bytes[at + i]); };
    const std::string_view type = kTypes[(b(0) & 0x1FU) % kTypes.size()];
    const int x = static_cast<std::int8_t>(b(1));
    const int y = static_cast<std::int8_t>(b(2));
    const int w = 1 + (b(3) & 0x1FU);
    const int h = 1 + (b(4) & 0x1FU);
    ((b(0) & 0x40U) != 0 ? sub : top).add(type, x, y, w, h, (b(0) & 0x20U) != 0);
  }
}

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  hefesto::cmd::ScriptSession script{project};
  hefesto::cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  hefesto::cmd::Dispatcher dispatcher{registry, services.view(), project, script};

  bool run(std::string_view line) {
    bool ok = true;
    for (const auto& r : hefesto::ui::run_command_line(dispatcher, line, hefesto::cmd::Origin::script)) {
      ok = ok && r.ok();
    }
    return ok;
  }
};

const std::string& exttest_text() {
  static const std::string text = hefesto::fuzz::source_file("tests/fuzz/data/exttest_fuzz.tech");
  return text;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto [mode, rest] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  CellText top;
  CellText sub;
  decode(rest, top, sub);
  constexpr std::array<int, 4> kOffsets{0, 3, 7, 16};
  const std::string uses = use_text("s0", kOffsets[(mode >> 4U) & 3U], false) + use_text("s1", 60, (mode & kArray) != 0);

  Session s;
  s.project.compat.magic_compat = (mode & kCompatOff) == 0;
  if (!hefesto::cmd::register_all_commands(s.registry)) {
    return 0;
  }
  s.dispatcher.set_editor(&s.editor);
  hefesto::platform::FileSystem& fs = s.services.view().files;
  (void)fs.create_directories("/w");
  if (!fs.write_atomic("/w/exttest.tech", exttest_text()).ok() || !fs.write_atomic("/w/sub.mag", sub.mag("")).ok() ||
      !fs.write_atomic("/w/top.mag", top.mag(uses)).ok()) {
    return 0;
  }
  if (!s.run("tech load /w/exttest -noprompt") || !s.run("addpath /w") || !s.run("load top")) {
    return 0;
  }
  s.editor.drc.compat = &s.project.compat;
  if (s.editor.windows.current() == nullptr &&
      !s.editor.windows.open(s.editor.cells.find("top"), hefesto::view::ScreenRect{0, 0, 99, 99}).ok()) {
    return 0;
  }
  (void)s.run("box 0 0 1 1");
  if ((mode & kCorners) != 0) {
    (void)s.run("extract do corners");
    (void)s.run("extract no adjust");
  }
  (void)s.run("extract all");
  (void)s.run("ext2spice -p /w -o /w/top.spice top");
  if ((mode & kExtresist) != 0) {
    (void)s.run("extresist all");
  }
  return 0;
}
