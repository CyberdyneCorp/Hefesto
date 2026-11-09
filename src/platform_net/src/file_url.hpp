// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// file:// URLs of the HTTP Fetcher (local static-web catalogs and assets),
// read from disk without a network transport.

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::platform_net::detail {

/// The path of a `file:///abs/path` or `file://localhost/abs/path` URL with
/// %XX escapes decoded; nullopt for another host or a malformed escape.
[[nodiscard]] std::optional<platform::Path> file_url_path(std::string_view url);

/// The whole file; PDK-FETCH-002 "not found: <url>" when it does not exist.
[[nodiscard]] diag::Outcome<std::string> read_file_url(std::string_view url);

/// Copies the file to `to`, resuming a shorter partial copy like a range
/// request (a longer one is restarted), with progress per chunk.
[[nodiscard]] diag::Status copy_file_url(std::string_view url, const platform::Path& to,
                                         const std::function<void(std::uint64_t, std::uint64_t)>& progress);

}  // namespace hefesto::platform_net::detail
