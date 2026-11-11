// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/script/events.hpp"

#include <algorithm>

namespace hefesto::script {

void ScriptEvents::add(ScriptObserver& observer) {
  if (std::find(observers_.begin(), observers_.end(), &observer) == observers_.end()) {
    observers_.push_back(&observer);
  }
}

void ScriptEvents::remove(const ScriptObserver& observer) {
  std::erase(observers_, &observer);
}

void ScriptEvents::publish_denied(const DeniedOperation& operation) {
  for (auto* observer : observers_) {
    observer->on_denied(operation);
  }
}

void ScriptEvents::publish_skipped_statement(const SkippedStatement& statement) {
  for (auto* observer : observers_) {
    observer->on_skipped_statement(statement);
  }
}

void ScriptEvents::publish_helper_redefined(const HelperRedefinition& redefinition) {
  for (auto* observer : observers_) {
    observer->on_helper_redefined(redefinition);
  }
}

void ScriptEvents::publish_consent_requested(const ConsentRequest& request) {
  for (auto* observer : observers_) {
    observer->on_consent_requested(request);
  }
}

void ScriptEvents::publish_menu_model_changed() {
  for (auto* observer : observers_) {
    observer->on_menu_model_changed();
  }
}

}  // namespace hefesto::script
