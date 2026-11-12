// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-cli's interactive prompt (m3-drc design D8 "Hosts"): the
// background checker runs while the prompt waits for input and its
// busy/idle transitions switch Magic's prompt between ']' and '%'; lines
// continue while a brace group is open (Tcl's `info complete`); `quit` and
// end of input end it.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/drc/engine/kernel_hook.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/binding/bind.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/tech/technology.hpp"
#include "interactive.hpp"
#include "options.hpp"

#include <unistd.h>

namespace cli = hefesto::cli;
namespace cmd = hefesto::cmd;
namespace drc = hefesto::drc;

#define PROMPT_TAGS "[cap:drc-engine][req:Pending list and background scheduling]"

namespace {

std::string slurp(std::FILE* f) {
  std::fflush(f);
  std::rewind(f);
  std::string text;
  char buffer[4096];
  std::size_t n = 0;
  while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0) {
    text.append(buffer, n);
  }
  return text;
}

/// A dispatcher over a one-plane technology (`m1` on plane `metal1`).
struct Console {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  std::FILE* prompt = std::tmpfile();
  int fds[2] = {-1, -1};

  Console() {
    hefesto::tech::TechnologyBuilder b;
    b.set_name("clit");
    REQUIRE(b.add_plane("metal1").ok());
    REQUIRE(b.add_type("metal1", "m1").ok());
    editor.technology = b.finalize().value();
    // An edit cell shown in a window, as after the CLI's startup.
    hefesto::db::CellDef* top = editor.cells.create("top", editor.technology->plane_count()).value();
    top->flags().available = true;
    editor.edit_cell = top;
    REQUIRE(editor.windows.open_frame(top).ok());
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    // Typed commands are Tcl in the project's interpreter (m4-scripting 13.2).
    hefesto::script::Interpreter& interp = hefesto::script::open_project_interpreter(editor, {});
    REQUIRE(hefesto::script::bind_registry(interp, registry, *dispatcher).ok());
    REQUIRE(prompt != nullptr);
    REQUIRE(::pipe(fds) == 0);
  }
  ~Console() {
    hefesto::script::close_project_interpreter(editor);
    std::fclose(prompt);
    if (fds[0] >= 0) {
      ::close(fds[0]);
    }
    if (fds[1] >= 0) {
      ::close(fds[1]);
    }
  }
  Console(const Console&) = delete;
  Console& operator=(const Console&) = delete;

  void type(std::string_view text) const { REQUIRE(::write(fds[1], text.data(), text.size()) == static_cast<ssize_t>(text.size())); }
  void end_input() {
    ::close(fds[1]);
    fds[1] = -1;
  }
  /// The prompt text so far (written by the loop thread; read after it waits).
  std::string prompt_text() const {
    std::fflush(prompt);
    std::string text;
    const long end = std::ftell(prompt);
    std::rewind(prompt);
    text.resize(static_cast<std::size_t>(end));
    text.resize(std::fread(text.data(), 1, text.size(), prompt));
    std::fseek(prompt, 0, SEEK_END);
    return text;
  }
};

/// Polls `predicate` until true or 20 s.
template <typename Predicate>
bool eventually(Predicate predicate) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  while (!predicate()) {
    if (std::chrono::steady_clock::now() > deadline) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return true;
}

// A kernel that holds the first square until the test opens the gate.
struct Gate {
  std::mutex mutex;
  std::condition_variable cv;
  bool entered = false;
  bool open = false;

  static Gate& get() {
    static Gate gate;
    return gate;
  }
};

drc::CheckOutcome gated_kernel(const drc::CheckInput& /*in*/, const hefesto::db::Rect& /*check*/,
                               const hefesto::db::Rect& /*clip*/, drc::ErrorSink& /*sink*/) {
  Gate& g = Gate::get();
  std::unique_lock lock(g.mutex);
  g.entered = true;
  g.cv.notify_all();
  g.cv.wait(lock, [&] { return g.open; });
  return drc::CheckOutcome::completed;
}

}  // namespace

TEST_CASE("The interactive prompt shows the busy prompt while the background checker runs", PROMPT_TAGS) {
  Gate& g = Gate::get();
  {
    const std::lock_guard lock(g.mutex);
    g.entered = false;
    g.open = false;
  }
  const drc::ScopedKernel kernel(&gated_kernel);
  Console console;
  cli::InteractiveRun result;
  std::thread loop([&] { result = cli::run_interactive(*console.dispatcher, console.editor, console.fds[0], console.prompt, true); });

  REQUIRE(eventually([&] { return console.prompt_text() == "% "; }));  // idle, waiting for input
  // The worker runs between commands: the paint marks the box and the
  // checker turns busy while the prompt waits.
  console.type("box 0 0 10 10\npaint m1\n");
  {
    std::unique_lock lock(g.mutex);
    REQUIRE(g.cv.wait_for(lock, std::chrono::seconds(20), [&] { return g.entered; }));
  }
  CHECK(eventually([&] { return console.prompt_text().ends_with("] "); }));
  {
    const std::lock_guard lock(g.mutex);
    g.open = true;
  }
  g.cv.notify_all();
  // The square is committed on the loop's thread; idle rewrites the prompt.
  CHECK(eventually([&] { return console.prompt_text().ends_with("\b\b  \b\b% "); }));
  console.type("quit -noprompt\n");
  loop.join();
  CHECK(result.quit);
  CHECK(result.ok);
  CHECK(console.editor.drc.pending.empty());
  CHECK_FALSE(console.editor.drc.worker->running());  // stopped when the prompt ends
  CHECK(console.prompt_text() == "% ] \b\b  \b\b% ");
}

TEST_CASE("The interactive prompt continues a line with an open brace group", PROMPT_TAGS) {
  Console console;
  std::FILE* out = std::tmpfile();
  REQUIRE(out != nullptr);
  struct Echo final : cmd::CommandObserver {
    std::FILE* out;
    explicit Echo(std::FILE* f) : out(f) {}
    void on_command(const cmd::Invocation& /*invocation*/, const cmd::CommandResult& r) override {
      std::fputs((r.value.to_string() + "\n").c_str(), out);
    }
  } echo(out);
  console.dispatcher->add_observer(echo);
  console.type("magic::echo {a\nb}\nnosuchcommand\nmagic::echo after\n");
  console.end_input();
  const cli::InteractiveRun result =
      cli::run_interactive(*console.dispatcher, console.editor, console.fds[0], console.prompt, false);
  CHECK_FALSE(result.quit);  // ended by end of input
  CHECK_FALSE(result.ok);    // a command failed, and the prompt went on
  CHECK(slurp(out) == "a\nb\nafter\n");  // the Tcl error does not reach the dispatcher
  CHECK(console.prompt_text().empty());  // no prompt when disabled
  std::fclose(out);
}

TEST_CASE("Option --interactive selects the interactive prompt", "[cap:hefesto-architecture][req:Headless command-line tool]") {
  const std::vector<std::string_view> args{"--interactive", "-c", "echo hi"};
  const cli::ParsedArgs parsed = cli::parse_args(args);
  REQUIRE_FALSE(parsed.error);
  CHECK(parsed.options.interactive);
  CHECK(parsed.options.sources.size() == 1);  // -c runs first, then the prompt
  const std::vector<std::string_view> batch{"-c", "echo hi"};
  CHECK_FALSE(cli::parse_args(batch).options.interactive);  // batch: no prompt, no worker
  CHECK(cli::usage_text().find("--interactive") != std::string::npos);
}
