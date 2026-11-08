// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Test double: a complete headless command session over in-memory services.

#pragma once

#include <memory>
#include <string>

#include "hefesto/cmd/builtins.hpp"
#include "hefesto/cmd/command_log.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/script.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/platform/services.hpp"

namespace hefesto::test {

/// Registers a command `fail` that fails with CMD-USAGE-001 (a stand-in for
/// `load nonexistent -fail` until `load` exists) and `count` that counts calls.
struct TestCommands {
  int count = 0;

  void add_to(cmd::CommandRegistry& registry) {
    (void)registry.add({"fail", "fail", "Always fails.", [](cmd::CommandContext&, const cmd::Invocation&) {
                          return cmd::CommandResult::failure(diag::make(diag::codes::kCmdUsage, "it failed"));
                        }});
    (void)registry.add({"count", "count", "Counts calls.", [this](cmd::CommandContext&, const cmd::Invocation&) {
                          return cmd::CommandResult::success(++count);
                        }});
  }
};

class Session {
 public:
  explicit Session(settings::ProjectSettings project = {})
      : services_(platform::PortableOptions{.storage_root = "/app", .pdk_roots = {"/pdk"}, .in_memory = true,
                                            .inline_tasks = true}),
        project_(project),
        session_(project_) {
    auto logger = std::make_unique<platform::MemoryLogger>();
    logger_ = logger.get();
    services_.set_logger(std::move(logger));
    cmd::register_builtins(registry_);
    commands_.add_to(registry_);
    dispatcher_ = std::make_unique<cmd::Dispatcher>(registry_, services_.view(), project_, session_);
    log_ = std::make_unique<cmd::CommandLog>(logger_);
    dispatcher_->add_observer(*log_);
  }

  cmd::CommandResult exec(std::string name, std::vector<std::string> args = {}) {
    return dispatcher_->execute({std::move(name), std::move(args), cmd::Origin::script, std::nullopt});
  }
  cmd::ScriptRun script(std::string_view text, std::string_view source = {}) {
    return cmd::run_script(*dispatcher_, text, cmd::Origin::script, source);
  }

  cmd::Dispatcher& dispatcher() { return *dispatcher_; }
  cmd::CommandRegistry& registry() { return registry_; }
  cmd::ScriptSession& session() { return session_; }
  settings::ProjectSettings& project() { return project_; }
  platform::MemoryLogger& logger() { return *logger_; }
  cmd::CommandLog& log() { return *log_; }
  platform::PortableServices& services() { return services_; }
  TestCommands& commands() { return commands_; }

 private:
  platform::PortableServices services_;
  platform::MemoryLogger* logger_ = nullptr;
  settings::ProjectSettings project_;
  cmd::ScriptSession session_;
  cmd::CommandRegistry registry_;
  TestCommands commands_;
  std::unique_ptr<cmd::Dispatcher> dispatcher_;
  std::unique_ptr<cmd::CommandLog> log_;
};

}  // namespace hefesto::test
