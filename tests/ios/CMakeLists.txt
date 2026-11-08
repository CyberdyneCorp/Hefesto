# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# iPadOS link probe (capability hefesto-architecture, scenario "Core builds
# without UI": the core compiles AND links on iPadOS). Archiving the static
# hefesto_core runs no link step, so this tiny app links the core into an
# iOS executable; unresolved or iOS-unavailable symbols fail the build.
# Built by CI on the macOS legs, unsigned:
#   cmake --build build/ipados --target hefesto_ios_link_probe --config Debug -- CODE_SIGNING_ALLOWED=NO
# Never shipped; the iPadOS app starts with M14.

add_executable(hefesto_ios_link_probe MACOSX_BUNDLE link_probe.cpp)
target_link_libraries(hefesto_ios_link_probe PRIVATE hefesto::core)
set_target_properties(hefesto_ios_link_probe PROPERTIES
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.hefesto.ios-link-probe"
  MACOSX_BUNDLE_BUNDLE_NAME "HefestoLinkProbe"
  MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.hefesto.ios-link-probe"
  XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
hefesto_set_warnings(hefesto_ios_link_probe)
