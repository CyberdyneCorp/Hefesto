// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M7 round 6 (tasks.md 16.1, design D19): `extract` and `extract all` run
// through the desktop's progress host (MaskSession::write_host) when one is
// set, so the top bar's Extract button gets the M5 running header,
// Interrupt card and cancel. Without a host (CLI, tests) they run inline.
// Owner: SHELL (the host route); EXTCMD (the command).

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/mask/gen/progress.hpp"
#include "run_fixture.hpp"

using extcmd_test::mag;
using extcmd_test::Session;

namespace {

const std::string kLeaf = mag("<< metal1 >>\nrect 0 0 10 4\n<< labels >>\nrlabel metal1 2 2 2 2 0 a\n");

/// A host that records its labels and runs the work with `progress`.
struct RecordingHost {
  std::vector<std::string> labels;
  std::atomic<bool> cancel{false};
  hefesto::mask::Progress progress;

  void install(Session& s) {
    progress.cancel = &cancel;
    s.editor.mask.write_host = [this](std::string_view label, const hefesto::mask::ProgressWork& work) {
      labels.emplace_back(label);
      return work(&progress);
    };
  }
};

}  // namespace

TEST_CASE("extract all runs through the desktop's progress host",
          "[cap:extract-commands][req:Extract all, cell, parents and showparents]") {
  Session s;
  s.load("leaf", kLeaf);
  RecordingHost host;
  host.install(s);
  REQUIRE(s.run("extract all").ok());
  CHECK(host.labels == std::vector<std::string>{"leaf.ext"});
  CHECK(s.exists("leaf.ext"));
  CHECK(s.take_printed().find("Extracting leaf into /w/leaf.ext:") != std::string::npos);
}

TEST_CASE("an Interrupt from the host keeps the previous .ext",
          "[cap:extract-commands][req:Incremental extraction]") {
  Session s;
  s.load("leaf", kLeaf);
  s.file("leaf.ext", "timestamp 1\nprevious\n");
  RecordingHost host;
  host.install(s);
  host.cancel = true;  // the card's Interrupt before the first cell
  REQUIRE(s.run("extract all").ok());
  CHECK(host.labels == std::vector<std::string>{"leaf.ext"});
  CHECK(s.read("leaf.ext") == "timestamp 1\nprevious\n");
  CHECK_FALSE(s.exists(".leaf.ext.hefesto-tmp"));
  s.editor.mask.write_host = {};  // no host: inline, as the CLI
  REQUIRE(s.run("extract all").ok());
  CHECK(s.read("leaf.ext").rfind("timestamp 100", 0) == 0);
}
