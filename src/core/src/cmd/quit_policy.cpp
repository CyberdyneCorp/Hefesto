// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/cmd/quit_policy.hpp"

#include "hefesto/cmd/context.hpp"

namespace hefesto::cmd {

CommandResult ExitQuitPolicy::quit(CommandContext& ctx, const Invocation& /*invocation*/, const QuitRequest& request) {
  ScriptSession& session = ctx.session();
  session.exit_status = request.status;
  session.quit_requested = true;
  return CommandResult::success();
}

}  // namespace hefesto::cmd
