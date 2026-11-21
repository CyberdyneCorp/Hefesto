// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The token scanner of "Format string substitution" (capability
// schematic-netlisting), shared by the element builder and the
// hierarchical-parameter substitution. Internal to schem/net.
//
// Owner: NETLIST.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace hefesto::schem::detail {

inline bool is_format_separator(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == ';';
}

inline bool is_sigil(char c) noexcept {
  return c == '@' || c == '%';
}

/// Scans `text`: characters outside tokens go to `on_text(char)`, tokens to
/// `on_token(char sigil, std::string key, std::string_view source)` where
/// `source` is the token as written. With `escapes`, a backslash escapes the
/// next character and is dropped, an escaped LF is dropped entirely, and a
/// token ends before an escaped character other than a separator, `@` or
/// `%`; without, backslashes are ordinary characters. A token ends, from its
/// third character on, before an unescaped separator, `@` or `%`. The
/// character that ends a token is copied unless it is `@`, `%` or the end.
template <typename OnText, typename OnToken>
class FormatScanner {
 public:
  FormatScanner(std::string_view text, bool escapes, OnText& on_text, OnToken& on_token)
      : text_{text}, escapes_{escapes}, on_text_{on_text}, on_token_{on_token} {}

  void run() {
    std::size_t i = 0;
    while (i < text_.size()) {
      const char c = text_[i];
      if (escapes_ && c == '\\') {
        if (i + 1 < text_.size() && text_[i + 1] != '\n') {
          on_text_(text_[i + 1]);
        }
        i += 2;
      } else if (is_sigil(c)) {
        i = token(i);
      } else {
        on_text_(c);
        ++i;
      }
    }
  }

 private:
  /// Reads the token at `start`; returns where scanning continues.
  std::size_t token(std::size_t start) {
    std::string key;
    std::size_t k = start + 1;
    while (k < text_.size() && !ends_at(k, k - start)) {
      k = take(k, key);
    }
    on_token_(text_[start], std::move(key), text_.substr(start, k - start));
    return after_end(k);
  }

  [[nodiscard]] bool ends_at(std::size_t k, std::size_t position) const noexcept {
    if (position < 2) {
      return false;
    }
    const char c = text_[k];
    if (escapes_ && c == '\\' && k + 1 < text_.size()) {
      const char e = text_[k + 1];
      return !is_format_separator(e) && !is_sigil(e);
    }
    return is_format_separator(c) || is_sigil(c);
  }

  /// Appends the character at `k` (resolving an escape) to `key`.
  std::size_t take(std::size_t k, std::string& key) const {
    if (escapes_ && text_[k] == '\\') {
      if (k + 1 < text_.size() && text_[k + 1] != '\n') {
        key += text_[k + 1];
      }
      return k + 2;
    }
    key += text_[k];
    return k + 1;
  }

  std::size_t after_end(std::size_t k) {
    if (k >= text_.size() || is_sigil(text_[k])) {
      return k;
    }
    if (escapes_ && text_[k] == '\\' && k + 1 < text_.size()) {
      on_text_(text_[k + 1]);
      return k + 2;
    }
    on_text_(text_[k]);
    return k + 1;
  }

  std::string_view text_;
  bool escapes_;
  OnText& on_text_;
  OnToken& on_token_;
};

template <typename OnText, typename OnToken>
void scan_format(std::string_view text, bool escapes, OnText&& on_text, OnToken&& on_token) {
  FormatScanner<std::remove_reference_t<OnText>, std::remove_reference_t<OnToken>> scanner{text, escapes, on_text,
                                                                                           on_token};
  scanner.run();
}

}  // namespace hefesto::schem::detail
