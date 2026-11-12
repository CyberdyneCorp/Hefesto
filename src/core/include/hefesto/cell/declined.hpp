// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Layout opened read-only because its technology's PDK startup script was
// declined (hefesto-security "Consent before implicit script execution";
// m4-scripting design D7/D14; board M4-Script-Consent tile 6). Such a cell
// is read with the `minimum` technology, so the PDK's layers are not in
// memory: it is marked no_edit like any read-only cell and remembered here,
// so that no command writes it (also after `cellname writeable` or a
// `save <other name>`, which would replace the PDK layout with an empty
// `minimum` one). Once the scripts may run, reopen_declined() runs the
// PDK's script and reads the cells again.
//
// Owner: PDKINT (m4-scripting design D2; the CELL reader marks the cells).

#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/script/types.hpp"

namespace hefesto::cmd {
class CommandContext;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::cell {

/// The session's declined cells (CellSession::declined). A record holds for
/// the definition read from that file only: a definition deleted and
/// another created in its place, or the cell read again from another file,
/// is not declined.
class DeclinedCells {
 public:
  /// `def` was read from its file while the scripts of `technology` were declined.
  void mark(const db::CellDef& def, std::string technology);
  void forget(const db::CellDef& def) { cells_.erase(&def); }
  void clear() noexcept { cells_.clear(); }
  /// The declined technology (`sky130A`) of `def`, nullopt when it is not declined.
  [[nodiscard]] std::optional<std::string> technology_of(const db::CellDef& def) const;
  [[nodiscard]] bool contains(const db::CellDef& def) const { return technology_of(def).has_value(); }
  [[nodiscard]] bool empty() const noexcept { return cells_.empty(); }

 private:
  struct Record {
    std::string technology;
    platform::Path file;  // the definition's recorded path when it was read
  };
  std::map<const db::CellDef*, Record> cells_;
};

/// The definitions of the session's library that are declined with
/// `technology` (every declined one when it is empty), in library order.
[[nodiscard]] std::vector<db::CellDef*> declined_definitions(const editor::EditorState& editor,
                                                             const std::string& technology = {});

/// True when `def` (a window's root cell) is a declined cell of the
/// session's library. The desktop shows the read-only chip and disables
/// the editing tools then.
[[nodiscard]] bool is_declined(const editor::EditorState& editor, const db::CellDef& def);

/// Review scripts... said yes (board M4-Script-Consent note 6): forgets the
/// session's decline of `pdk`, runs `script` (the PDK's rc file) through
/// the consent path (the session TrustStore's host, which now answers run
/// once or trust), and reads every cell declined with `pdk`'s variant again
/// from its file with the technology the script loaded, the current
/// window's root becoming the edit cell again. Cells stay declined when the
/// script did not load that technology (SCRIPT-TRUST-006).
[[nodiscard]] cmd::CommandResult reopen_declined(cmd::CommandContext& ctx, const script::PdkIdentity& pdk,
                                                 const platform::Path& script);

}  // namespace hefesto::cell
