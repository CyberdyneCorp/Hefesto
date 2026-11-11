// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/script/pdk/consent.hpp"

namespace hefesto::script {

bool BatchConsentHost::matches(const PdkIdentity& pdk) const {
  for (const std::string& selector : selectors_) {
    if (selector == pdk.family || selector == pdk.variant || selector == pdk.to_string()) {
      return true;
    }
    const std::size_t at = selector.find('@');
    if (at != std::string::npos && at + 1 < selector.size() && selector.substr(0, at) == pdk.variant &&
        pdk.version.starts_with(selector.substr(at + 1))) {
      return true;
    }
  }
  return false;
}

ConsentDecision BatchConsentHost::ask(const PdkIdentity& pdk, const platform::Path& script) {
  static_cast<void>(script);
  return matches(pdk) ? ConsentDecision::run_once : ConsentDecision::decline;
}

ConsentDecision TestConsentHost::ask(const PdkIdentity& pdk, const platform::Path& script) {
  requests_.push_back(ConsentRequest{pdk, script.string(), false});
  if (answers_.empty()) {
    return fallback_;
  }
  const ConsentDecision answer = answers_.front();
  answers_.pop_front();
  return answer;
}

}  // namespace hefesto::script
