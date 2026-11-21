// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (tasks.md 5.2): read_document fails with CORE-NOTIMPL-007;
// write_document returns an empty text.
//
// Owner: FORMAT.

#include "hefesto/schem/file/read_write.hpp"

#include "hefesto/schem/not_implemented.hpp"

namespace hefesto::schem {

diag::Outcome<Document> read_document(std::string_view /*bytes*/, const ReadOptions& /*options*/) {
  return not_implemented("schem::read_document");
}

std::string write_document(const Document& /*document*/) {
  return {};
}

}  // namespace hefesto::schem
