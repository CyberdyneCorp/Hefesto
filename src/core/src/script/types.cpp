// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/script/types.hpp"

namespace hefesto::script {

std::string PdkIdentity::to_string() const {
  return family + "/" + variant + "@" + version;
}

}  // namespace hefesto::script
