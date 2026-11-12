// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop platform::FilePicker host (openspec/changes/m4-scripting task
// 10.5; ui-gui-managers "File prompts").

#include <catch2/catch_test_macros.hpp>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/shell/file_picker_host.hpp"

using namespace hefesto;
using namespace hefesto::shell;

TEST_CASE("a FilePicker request becomes the native dialog request",
          "[cap:ui-gui-managers][req:File prompts]") {
  platform::FilePickerRequest open;
  open.title = "Load Magic cell";
  open.initial_directory = platform::Path{"/work"};
  open.file_types = {{"Magic files", "*.mag"}, {"SPICE netlists", "*.spice *.sp *.cir"}};
  const NativeFileDialogRequest request = native_request(open);
  CHECK_FALSE(request.save);
  CHECK(request.title == "Load Magic cell");
  CHECK(request.directory == "/work");
  REQUIRE(request.filters.size() == 3);
  CHECK(request.filters[1] == NativeFileFilter{"SPICE netlists", "*.spice;*.sp;*.cir"});
  CHECK(request.filters[2] == NativeFileFilter{"All files", "*"});
  CHECK(request.default_extension.empty());

  platform::FilePickerRequest save;
  save.mode = platform::FilePickerRequest::Mode::save;
  save.initial_file = "top.gds";
  save.file_types = {{"GDS files", "*.gds *.gds2"}, {"All files", "*"}};
  const NativeFileDialogRequest saved = native_request(save);
  CHECK(saved.save);
  CHECK(saved.file_name == "top.gds");
  CHECK(saved.filters.size() == 2);  // "All files" is not added twice
  CHECK(saved.default_extension == "gds");
}

TEST_CASE("the desktop picker answers with the native dialog's path or a cancel",
          "[cap:ui-gui-managers][req:File prompts]") {
  std::vector<NativeFileDialogRequest> asked;
  std::optional<std::string> answer = "/work/inv.mag";
  DesktopFilePicker picker([&](const NativeFileDialogRequest& r) {
    asked.push_back(r);
    return answer;
  });
  platform::FilePicker& host = picker;
  CHECK(host.pick(platform::FilePickerRequest{}) == std::optional<platform::Path>{platform::Path{"/work/inv.mag"}});
  answer.reset();
  CHECK_FALSE(host.pick(platform::FilePickerRequest{}));  // cancelled
  answer = "";
  CHECK_FALSE(host.pick(platform::FilePickerRequest{}));  // an empty answer is a cancel
  CHECK(picker.asked() == 3);
  CHECK(asked.size() == 3);
  DesktopFilePicker none({});
  CHECK_FALSE(none.pick(platform::FilePickerRequest{}));
}
