// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Where the gencell framework uses a native generator instead of the PDK's
// Tcl device procedures (design D10; capability hefesto-scripting "Native
// PCell generators validated against Tcl"): only when
//  - the PDK's own device scripts are loaded (`<lib>::<type>_draw` exists),
//  - the PDK whose script defined `magic::spice2float` (the helpers every
//    generator calls; Q-SCRIPT-41's origin tracking) is one exact version a
//    native generator is registered for, and
//  - `::hefesto::native_generators` is not set to a false value (the
//    equivalence sweep sets it to 0 to run the Tcl generators).
// Another version of a PDK that has native generators runs the Tcl ones
// and reports PCELL-NATIVE-001 once per device and session.
//
// The NativeHost runs engine commands as the PDK's Tcl would: nested, not
// logged, a failure raising only when Magic's status convention raises it
// (a raising command, or any failure in strict mode).

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pcell/native/registry.hpp"
#include "hefesto/script/binding/output.hpp"
#include "hefesto/script/interp.hpp"
#include "pcell/framework/codes.hpp"
#include "pcell/framework/toolkit.hpp"
#include "pcell/native/tcl_value.hpp"
#include "ui/command_util.hpp"

namespace hefesto::pcell::detail {

namespace {

constexpr std::string_view kSwitch = "::hefesto::native_generators";
constexpr std::string_view kHelperOrigin = "::magic::spice2float";

class ToolkitHost final : public NativeHost {
 public:
  explicit ToolkitHost(Toolkit& tk) : tk_(tk) {}

  std::string run(const std::vector<std::string>& words) override {
    const cmd::CommandResult r = ui::detail::run_forwarded(tk_.context(), words);
    if (!r.ok() && (r.raises || tk_.context().session().strict)) {
      throw native::TclError{error_text(r)};
    }
    return r.ok() ? value_text(r.value) : std::string{};
  }

  void puts(bool to_stderr, std::string_view text) override {
    editor::EditorState* editor = tk_.context().editor();
    if (editor == nullptr) {
      return;
    }
    static_cast<void>(script::write_output(tk_.interp(), editor->script.output,
                                           to_stderr ? script::OutputStream::err : script::OutputStream::out, text));
  }

  void set_variable(std::string_view name, std::string_view value) override {
    static_cast<void>(tk_.interp().set_var("::" + std::string{name}, cmd::Value{std::string{value}}));
  }

 private:
  Toolkit& tk_;
};

bool switched_off(script::Interpreter& interp) {
  const auto value = interp.get_var(kSwitch);
  if (!value) {
    return false;
  }
  try {
    return !native::Val{value_text(*value)}.truth();
  } catch (const native::TclError&) {
    return false;
  } catch (const native::Unsupported&) {
    return false;
  }
}

}  // namespace

const NativeGenerator* Toolkit::native(std::string_view library, std::string_view type) {
  editor::EditorState* editor = ctx_->editor();
  if (editor == nullptr || switched_off(*interp_) || !has_device(library, type, "draw")) {
    return nullptr;
  }
  const auto pdk = editor->script.helper_origins.origin(kHelperOrigin);
  if (!pdk) {
    return nullptr;
  }
  const std::string device = std::string{library} + "::" + std::string{type};
  const NativeRegistry& registry = builtin_generators();
  if (const NativeGenerator* generator = registry.find(*pdk, device)) {
    return generator;
  }
  if (registry.has_other_version(*pdk, device) && state_->native_fallbacks.insert(device).second) {
    // Hefesto-only: Magic has no native generators, so its console never shows this.
    ctx_->emit(diag::make_native(codes::kNativeFallback, "No native generator of " + device + " for " + pdk->to_string() +
                                                       "; its Tcl generator runs."));
  }
  return nullptr;
}

std::optional<diag::Outcome<ParameterDict>> Toolkit::native_check(std::string_view library, std::string_view type,
                                                                  const ParameterDict& parameters) {
  const NativeGenerator* generator = native(library, type);
  if (generator == nullptr) {
    return std::nullopt;
  }
  ToolkitHost host{*this};
  return generator->check(host, parameters);
}

cmd::CommandResult Toolkit::draw_device(std::string_view library, std::string_view type,
                                        const ParameterDict& parameters) {
  if (const NativeGenerator* generator = native(library, type)) {
    ToolkitHost host{*this};
    if (auto drawn = generator->draw(host, parameters)) {
      return std::move(*drawn);
    }
  }
  return device(library, type, "draw", {parameters.to_tcl()});
}

}  // namespace hefesto::pcell::detail
