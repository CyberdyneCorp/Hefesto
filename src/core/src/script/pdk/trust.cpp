// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Trust and consent for PDK scripts (design D7; hefesto-security "Consent
// before implicit script execution").

#include "hefesto/script/pdk/trust.hpp"

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pdk/catalog.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/script/events.hpp"
#include "hefesto/script/pdk/consent.hpp"
#include "hefesto/script/project.hpp"

namespace hefesto::script {

namespace {

constexpr std::string_view kDeclined = "SCRIPT-TRUST-001";
constexpr std::string_view kTrusted = "SCRIPT-TRUST-002";
constexpr std::string_view kNotRecorded = "SCRIPT-TRUST-003";
constexpr std::string_view kUntrusted = "SCRIPT-TRUST-004";

class PdkStoreTrust final : public TrustBackend {
 public:
  PdkStoreTrust(platform::FileSystem& fs, pdk::InstallHost& host, const platform::Path& root)
      : fetcher_(pdk::make_unsupported_fetcher()), manager_(fs, host, *fetcher_, pdk::Catalog::builtin()) {
    manager_.set_root(root);
  }

  [[nodiscard]] bool trusted(const PdkIdentity& pdk) const override {
    const pdk::Family* family = manager_.catalog().find_family(pdk.family);
    if (family == nullptr || pdk.version.empty()) {
      return false;
    }
    const auto provenance = manager_.provenance(*family, pdk.version);
    return provenance.ok() && provenance.value().trusted;
  }

  [[nodiscard]] bool covers(const PdkIdentity& pdk, const platform::Path& script) const override {
    if (pdk.family.empty() || pdk.version.empty() || manager_.root().empty()) {
      return false;
    }
    std::error_code error;
    const platform::Path version = std::filesystem::weakly_canonical(
        manager_.root() / "ciel" / pdk.family / "versions" / pdk.version, error);
    const platform::Path file = std::filesystem::weakly_canonical(script, error);
    const platform::Path relative = file.lexically_relative(version);
    return !error && !relative.empty() && *relative.begin() != ".." && *relative.begin() != ".";
  }

  [[nodiscard]] diag::Status record(const PdkIdentity& pdk) override { return set(pdk, true); }
  [[nodiscard]] diag::Status forget(const PdkIdentity& pdk) override { return set(pdk, false); }

 private:
  diag::Status set(const PdkIdentity& pdk, bool trusted) {
    const pdk::Family* family = manager_.catalog().find_family(pdk.family);
    if (family == nullptr || pdk.version.empty()) {
      return diag::make(kNotRecorded, "The trust record of " + pdk.to_string() +
                                          " was not saved: not a version of the PDK store.");
    }
    return manager_.set_trusted(*family, pdk.version, trusted);
  }

  std::unique_ptr<pdk::Fetcher> fetcher_;
  pdk::PdkManager manager_;
};

std::string script_kind(bool implicit) {
  return implicit ? "technology startup script" : "startup script";
}

// The family of a variant directory name outside the ciel layout.
std::string family_of_variant(const std::string& variant) {
  static const pdk::Catalog catalog = pdk::Catalog::builtin();  // the selection points into it
  const auto selection = catalog.resolve(variant);
  return selection.ok() && selection.value().family != nullptr ? selection.value().family->name : variant;
}

}  // namespace

std::unique_ptr<TrustBackend> make_pdk_store_trust(platform::FileSystem& fs, pdk::InstallHost& host,
                                                   const platform::Path& root) {
  return std::make_unique<PdkStoreTrust>(fs, host, root);
}

bool TrustStore::trusted(const PdkIdentity& pdk) const {
  return trusted_.contains(pdk) || (backend_ != nullptr && backend_->trusted(pdk));
}

bool TrustStore::trusted(const PdkIdentity& pdk, const platform::Path& script) const {
  return trusted(pdk) && (backend_ == nullptr || backend_->covers(pdk, script));
}

void TrustStore::trust(const PdkIdentity& pdk) {
  trusted_.insert(pdk);
  declined_.erase(pdk);
}

bool TrustStore::declined_variant(std::string_view variant) const {
  for (const PdkIdentity& pdk : declined_) {
    if (pdk.variant == variant) {
      return true;
    }
  }
  return false;
}

void TrustStore::revoke(const PdkIdentity& pdk) {
  trusted_.erase(pdk);
}

ConsentOutcome request_consent(TrustStore& store, ScriptEvents& events, ConsentHost* host, const PdkIdentity& pdk,
                               const platform::Path& script, bool implicit) {
  ConsentOutcome out;
  if (store.trusted(pdk, script)) {
    out.run = true;
    return out;
  }
  ConsentDecision decision = ConsentDecision::decline;
  if (!store.declined(pdk)) {  // asked once per version and session
    events.publish_consent_requested(ConsentRequest{pdk, script.string(), implicit});
    ConsentHost* asked = host != nullptr ? host : store.consent_host();
    decision = asked != nullptr ? asked->ask(pdk, script) : ConsentDecision::decline;
  }
  if (decision == ConsentDecision::decline) {
    store.decline(pdk);
    out.diagnostics.push_back(diag::make(kDeclined, "The " + script_kind(implicit) + " \"" + script.string() +
                                                        "\" of " + pdk.to_string() +
                                                        " was not run (no consent); the layout opens read-only "
                                                        "with the minimum technology."));
    return out;
  }
  out.run = true;
  if (decision == ConsentDecision::trust_version) {
    store.trust(pdk);
    out.diagnostics.push_back(diag::make(kTrusted, pdk.to_string() + " is now trusted; its scripts run without asking."));
    if (TrustBackend* backend = store.backend()) {
      if (auto recorded = backend->record(pdk); !recorded) {
        out.diagnostics.push_back(diag::make(kNotRecorded, "Trust for " + pdk.to_string() +
                                                               " was not saved in the PDK store: " +
                                                               recorded.error().text));
      }
    }
  }
  return out;
}

namespace {

// `pdk trust` / `pdk untrust` on the session's store and `backend`.
cmd::CommandResult apply_trust(TrustStore& store, TrustBackend* backend, const PdkIdentity& pdk, TrustAction action) {
  const bool trust = action == TrustAction::trust;
  if (trust) {
    store.trust(pdk);
  } else {
    store.revoke(pdk);
    store.forget_decline(pdk);
  }
  cmd::CommandResult out = cmd::CommandResult::success();
  out.diagnostics.push_back(trust ? diag::make(kTrusted, pdk.to_string() + " is now trusted; its scripts run without asking.")
                                  : diag::make(kUntrusted, pdk.to_string() +
                                                               " is no longer trusted; its scripts ask before running."));
  if (backend != nullptr) {
    const auto saved = trust ? backend->record(pdk) : backend->forget(pdk);
    if (!saved) {
      out.diagnostics.push_back(diag::make(kNotRecorded, "The trust record of " + pdk.to_string() +
                                                             " was not saved in the PDK store: " + saved.error().text));
    }
  }
  return out;
}

}  // namespace

cmd::CommandResult run_pdk_trust(cmd::CommandContext& ctx, std::span<const std::string> words, TrustAction action,
                                 TrustBackend* fallback) {
  const std::string_view name = action == TrustAction::trust ? "trust" : "untrust";
  if (words.size() != 2 || words[0].empty() || words[1].empty()) {
    return cmd::CommandResult::failure(
        diag::make(diag::codes::kCmdUsage, "Usage: pdk " + std::string{name} + " <variant> <version>"));
  }
  auto editor = cmd::require_editor(ctx);
  if (!editor) {
    return cmd::CommandResult::failure(editor.error());
  }
  TrustStore& store = editor.value()->script.pdk_trust;
  const PdkIdentity pdk{family_of_variant(words[0]), words[0], words[1]};
  return apply_trust(store, store.backend() != nullptr ? store.backend() : fallback, pdk, action);
}

std::optional<PdkIdentity> pdk_identity_of(const platform::Path& file) {
  // Walk up to `<variant>/libs.tech/magic`.
  std::vector<std::string> parts;
  for (const auto& part : file.lexically_normal()) {
    parts.push_back(part.string());
  }
  for (std::size_t i = parts.size(); i-- > 2;) {
    if (parts[i] != "magic" || parts[i - 1] != "libs.tech") {
      continue;
    }
    PdkIdentity pdk;
    pdk.variant = parts[i - 2];
    if (pdk.variant == "/" || pdk.variant.empty()) {
      return std::nullopt;
    }
    const bool ciel = i >= 5 && parts[i - 4] == "versions";
    pdk.family = ciel ? parts[i - 5] : family_of_variant(pdk.variant);
    pdk.version = ciel ? parts[i - 3] : std::string{};
    return pdk;
  }
  return std::nullopt;
}

platform::Path pdk_root_for(const platform::Path& variant_dir) {
  platform::Path dir = variant_dir.lexically_normal();
  if (!dir.has_filename()) {
    dir = dir.parent_path();  // a trailing separator
  }
  return dir.parent_path();
}

}  // namespace hefesto::script
