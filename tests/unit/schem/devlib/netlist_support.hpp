// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Netlisting through the public schem APIs with the system library: the
// `xschemrc` files are evaluated in a sandboxed Tcl environment (the search
// path, and the variables `tcleval` values read), and the document is
// netlisted with that environment's Tcl bridge, as `schematic netlist` does.
//
// Owner: DEVLIB.

#pragma once

#include <string>
#include <vector>

#include "devlib_support.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/schem/file/save.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/lib/xschemrc.hpp"
#include "hefesto/schem/net/netlist.hpp"
#include "hefesto/schem/tcl/tcl_environment.hpp"

namespace hefesto::test::devlib {

/// A sandboxed Tcl environment whose `xschemrc` result is loaded.
struct NetlistBench {
  editor::EditorState editor;
  platform::LocalFileSystem files;
  schem::TclEnvironment tcl;
  schem::XschemrcResult rc;

  NetlistBench(const schem::XschemrcInput& input, std::vector<Path> read_only)
      : tcl(editor, policy(input, std::move(read_only)), files), rc(tcl.load_xschemrc(input)) {}

  /// The `xschemrc` sandbox (synthetic env(HOME), env(PWD), env(PDK_ROOT)),
  /// with `read_only` and the source tree readable too.
  static script::SandboxPolicy policy(const schem::XschemrcInput& input, std::vector<Path> read_only) {
    script::SandboxPolicy p = schem::xschemrc_policy(input);
    read_only.push_back(kSource);
    p.read_only_roots.insert(p.read_only_roots.end(), read_only.begin(), read_only.end());
    return p;
  }

  /// Netlists the file `top` with the defaults of "Netlist options".
  schem::NetlistResult netlist(const Path& top, const Path& pdk_root = {}, bool xschem_compat = true) {
    schem::ReadOptions read;
    read.xschem_compat = xschem_compat;
    auto document = schem::load_document(files, top, read);
    if (!document) return schem::NetlistResult{{}, {}, true, {}};
    schem::SymbolCache symbols;
    const schem::TclBridge bridge = tcl.bridge();
    const schem::NetlistEnvironment environment{symbols, rc.search_path, files, &bridge, pdk_root, 4};
    compat::CompatSettings compat;
    compat.xschem_compat = xschem_compat;
    return schem::netlist(environment, document.value(), schem::NetlistOptions{}, compat);
  }
};

/// The input of a document folder's own `xschemrc` with CMD's XSCHEM_SHAREDIR
/// and no PDK.
inline schem::XschemrcInput folder_input(const Path& folder) {
  schem::XschemrcInput input;
  input.document_folder = folder;
  input.variables = {{"XSCHEM_SHAREDIR", kSharedir.generic_string()},
                     {"USER_CONF_DIR", (folder / ".xschem").generic_string()}};
  return input;
}

inline std::string replace_all(std::string text, const std::string& from, const std::string& to) {
  if (from.empty()) return text;
  for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
    text.replace(at, from.size(), to);
  }
  return text;
}

inline std::vector<std::string> lines_of(const std::string& text) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start < text.size()) {
    const std::size_t end = text.find('\n', start);
    out.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return out;
}

}  // namespace hefesto::test::devlib
