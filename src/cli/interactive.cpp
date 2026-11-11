// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-cli's interactive prompt (m3-drc design D8 "Hosts"). POSIX:
// poll() waits on the input descriptor and on a self-pipe the background
// checker's ready callback writes to, so finished squares are committed on
// this (the command) thread between commands.

#include "interactive.hpp"

#include <array>
#include <cerrno>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/script.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/drc/engine/host.hpp"
#include "hefesto/editor/editor_state.hpp"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace hefesto::cli {

namespace {

/// A non-blocking self-pipe: the worker thread writes a byte, poll() wakes.
class WakePipe {
 public:
  WakePipe() {
    if (::pipe(fds_.data()) != 0) {
      fds_ = {-1, -1};
      return;
    }
    for (const int fd : fds_) {
      (void)::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
      (void)::fcntl(fd, F_SETFD, FD_CLOEXEC);
    }
  }
  ~WakePipe() {
    for (const int fd : fds_) {
      if (fd >= 0) {
        (void)::close(fd);
      }
    }
  }
  WakePipe(const WakePipe&) = delete;
  WakePipe& operator=(const WakePipe&) = delete;

  [[nodiscard]] int read_end() const noexcept { return fds_[0]; }

  /// Called on the worker thread (async-signal-safe calls only).
  void notify() const noexcept {
    const char byte = 'r';
    (void)!::write(fds_[1], &byte, 1);  // a full pipe already holds a wake-up
  }

  void drain() const noexcept {
    std::array<char, 64> sink{};
    while (::read(fds_[0], sink.data(), sink.size()) > 0) {
    }
  }

 private:
  std::array<int, 2> fds_{-1, -1};
};

/// Splits the bytes of a descriptor into lines.
class LineReader {
 public:
  explicit LineReader(int fd) : fd_(fd) {}

  /// The next complete line (without its newline); at end of input, the
  /// unterminated rest once.
  std::optional<std::string> take_line() {
    const auto newline = buffer_.find('\n');
    if (newline != std::string::npos) {
      std::string line = buffer_.substr(0, newline);
      buffer_.erase(0, newline + 1);
      return line;
    }
    if (eof_ && !buffer_.empty()) {
      return std::exchange(buffer_, std::string{});
    }
    return std::nullopt;
  }

  /// Reads what is available (the descriptor was reported readable).
  void fill() {
    std::array<char, 4096> chunk{};
    const ssize_t n = ::read(fd_, chunk.data(), chunk.size());
    if (n > 0) {
      buffer_.append(chunk.data(), static_cast<std::size_t>(n));
    } else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
      eof_ = true;
    }
  }

  [[nodiscard]] bool exhausted() const noexcept { return eof_ && buffer_.empty(); }
  [[nodiscard]] int fd() const noexcept { return fd_; }

 private:
  int fd_;
  std::string buffer_;
  bool eof_ = false;
};

/// Magic's console prompt (textio/txInput.c TxPrompt, TxUnPrompt).
class Prompt {
 public:
  Prompt(std::FILE* out, bool enabled) : out_(out), enabled_(enabled) {}

  /// Shows `ch` (no-op when it is already shown); a different prompt on
  /// screen is erased first.
  void show(char ch) {
    if (!enabled_ || shown_ == ch) {
      return;
    }
    if (shown_ != '\0') {
      std::fputs("\b\b  \b\b", out_);
    }
    std::fputc(ch, out_);
    std::fputc(' ', out_);
    std::fflush(out_);
    shown_ = ch;
  }

  /// The user ended a line: the prompt scrolled away with it.
  void consumed() noexcept { shown_ = '\0'; }

 private:
  std::FILE* out_;
  bool enabled_;
  char shown_ = '\0';
};

/// True while `text` needs another line: an open brace group or quote, or
/// a backslash-newline at its end.
bool incomplete(std::string_view text) {
  if (text.ends_with("\\\n")) {
    return true;
  }
  const auto parsed = cmd::parse_script(text);
  return parsed.error &&
         (parsed.error->code == diag::codes::kCmdParseBrace || parsed.error->code == diag::codes::kCmdParseQuote);
}

/// Waits for input or a finished square; nullopt when poll failed for
/// good, else whether the input is readable.
std::optional<bool> wait_for_event(const LineReader& input, const WakePipe& wake) {
  std::array<pollfd, 2> fds{pollfd{input.fd(), POLLIN, 0}, pollfd{wake.read_end(), POLLIN, 0}};
  const nfds_t count = wake.read_end() >= 0 ? 2 : 1;
  while (::poll(fds.data(), count, -1) < 0) {
    if (errno != EINTR) {
      return std::nullopt;
    }
  }
  if ((fds[1].revents & POLLIN) != 0) {
    wake.drain();
  }
  return (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) != 0;
}

class InteractiveLoop {
 public:
  InteractiveLoop(cmd::Dispatcher& dispatcher, editor::EditorState& editor, int input_fd, std::FILE* prompt_out,
                  bool show_prompt)
      : dispatcher_(dispatcher),
        host_(editor, [this] { wake_.notify(); }),
        input_(input_fd),
        prompt_(prompt_out, show_prompt) {}

  InteractiveRun run() {
    while (!run_.quit) {
      (void)host_.service();
      if (auto line = input_.take_line()) {
        add_line(*line);
      } else if (input_.exhausted()) {
        execute();  // an unterminated last command still runs (and reports its parse error)
        break;
      } else if (!wait()) {
        break;
      }
    }
    return run_;
  }

 private:
  void add_line(const std::string& line) {
    prompt_.consumed();
    text_ += line;
    text_ += '\n';
    if (!incomplete(text_)) {
      execute();
    }
  }

  void execute() {
    if (text_.empty()) {
      return;
    }
    const cmd::ScriptRun result = cmd::run_script(dispatcher_, std::exchange(text_, std::string{}), cmd::Origin::cli);
    run_.ok = run_.ok && result.failures == 0;
    run_.quit = result.quit || dispatcher_.session().quit_requested;
  }

  // Shows the prompt and blocks until input or a finished square.
  bool wait() {
    if (text_.empty()) {
      prompt_.show(host_.prompt_char());
    }
    const auto readable = wait_for_event(input_, wake_);
    if (readable && *readable) {
      input_.fill();
    }
    return readable.has_value();
  }

  cmd::Dispatcher& dispatcher_;
  WakePipe wake_;  // declared before host_: the worker's callback writes to it until host_ is gone
  drc::BackgroundHost host_;
  LineReader input_;
  Prompt prompt_;
  InteractiveRun run_;
  std::string text_;  // the command being assembled over continuation lines
};

}  // namespace

InteractiveRun run_interactive(cmd::Dispatcher& dispatcher, editor::EditorState& editor, int input_fd,
                               std::FILE* prompt_out, bool show_prompt) {
  InteractiveLoop loop(dispatcher, editor, input_fd, prompt_out, show_prompt);
  return loop.run();
}

}  // namespace hefesto::cli
