// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The per-PDK-version trust store and the consent decision
// (openspec/changes/m4-scripting/design.md D7; capability hefesto-security
// "Consent before implicit script execution", hefesto-pdk-manager "Script
// trust for managed PDKs"). A PDK script runs when its version is trusted
// (in this session, or recorded in the PDK store through
// pdk::Provenance::trusted), or when the ConsentHost approves it. "Trust
// this version" is recorded per PDK and version; trust never carries over to
// another version.
//
// editor::EditorState::script.pdk_trust is the session's TrustStore. The
// front end gives it the ConsentHost (desktop dialog, CLI policy, test host)
// and, when a PDK store is available, its TrustBackend before startup runs.
//
// Owner: PDKINT (design D2).

#pragma once

#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/script/types.hpp"

namespace hefesto::pdk {
class InstallHost;
}

namespace hefesto::script {

class ConsentHost;
class ScriptEvents;

/// Where trust is persisted (the PDK store's provenance records).
class TrustBackend {
 public:
  TrustBackend() = default;
  TrustBackend(const TrustBackend&) = delete;
  TrustBackend& operator=(const TrustBackend&) = delete;
  virtual ~TrustBackend() = default;

  [[nodiscard]] virtual bool trusted(const PdkIdentity& pdk) const = 0;
  /// Records trust for `pdk`; a failure is reported (SCRIPT-TRUST-003) and
  /// the session still trusts the version.
  [[nodiscard]] virtual diag::Status record(const PdkIdentity& pdk) = 0;
};

/// A backend over the PDK store at `root`: `Provenance::trusted` of the
/// version, recorded with PdkManager::set_trusted. `fs` and `host` are not
/// owned and must outlive the backend (the session's file system and
/// install host).
[[nodiscard]] std::unique_ptr<TrustBackend> make_pdk_store_trust(platform::FileSystem& fs, pdk::InstallHost& host,
                                                                 const platform::Path& root);

class TrustStore {
 public:
  /// Trusted in this session or by the backend.
  [[nodiscard]] bool trusted(const PdkIdentity& pdk) const;
  /// Trusts `pdk` for this session only.
  void trust(const PdkIdentity& pdk);
  void revoke(const PdkIdentity& pdk);
  /// The versions trusted in this session (not the backend's).
  [[nodiscard]] const std::set<PdkIdentity>& entries() const noexcept { return trusted_; }

  void set_backend(std::unique_ptr<TrustBackend> backend) noexcept { backend_ = std::move(backend); }
  [[nodiscard]] TrustBackend* backend() const noexcept { return backend_.get(); }
  /// The host asked when a script of an untrusted version would run; not
  /// owned (nullptr: every untrusted script is declined, the batch default).
  void set_consent_host(ConsentHost* host) noexcept { host_ = host; }
  [[nodiscard]] ConsentHost* consent_host() const noexcept { return host_; }

 private:
  std::set<PdkIdentity> trusted_;
  std::unique_ptr<TrustBackend> backend_;
  ConsentHost* host_ = nullptr;
};

/// The outcome of asking for consent.
struct ConsentOutcome {
  bool run = false;
  std::vector<diag::Diagnostic> diagnostics;  // SCRIPT-TRUST-* notes
};

/// The consent rule for `script` of `pdk` ("Consent before implicit script
/// execution"): trusted versions run without asking; otherwise the request
/// is published on `events`, `host` (or the store's host when `host` is
/// nullptr) is asked, and "trust this version" is recorded in `store` and
/// its backend. No host declines.
[[nodiscard]] ConsentOutcome request_consent(TrustStore& store, ScriptEvents& events, ConsentHost* host,
                                             const PdkIdentity& pdk, const platform::Path& script, bool implicit);

/// The PDK a script file belongs to, from its path. In the ciel and volare
/// layouts (`.../<family>/versions/<hash>/<variant>/libs.tech/magic/<file>`)
/// all three fields are known; in a plain open_pdks install
/// (`.../<variant>/libs.tech/magic/<file>`) the family is taken from the
/// built-in catalog and the version is empty (trust then applies to that
/// install path's unknown version). nullopt outside a `libs.tech/magic`
/// directory.
[[nodiscard]] std::optional<PdkIdentity> pdk_identity_of(const platform::Path& file);

/// The `PDK_ROOT` a PDK's own scripts expect for `pdk`'s variant directory
/// `variant_dir` (`<PDK_ROOT>/<variant>/libs.tech/magic/...`): its parent.
[[nodiscard]] platform::Path pdk_root_for(const platform::Path& variant_dir);

}  // namespace hefesto::script
