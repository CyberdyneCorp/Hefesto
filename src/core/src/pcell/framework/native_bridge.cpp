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

#include <functional>
#include <optional>
#include <unordered_set>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
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

  // The engine command as the PDK's Tcl calls it: `::magic::<name> ...`
  // evaluated by the interpreter, so its messages print when it runs (in
  // order with the PDK's own Tcl, as in Magic) and its tag fires.
  std::string run(const std::vector<std::string>& words) override {
    if (words.empty()) {
      return {};
    }
    std::vector<std::string> call{words};
    if (call.front().find("::") == std::string::npos) {
      call.front() = "::magic::" + call.front();
    }
    return evaluate(merge_tcl_list(call));
  }

  std::string evaluate(std::string_view script) override {
    const cmd::CommandResult r = tk_.interp().evaluate(std::string{script});
    if (!r.ok()) {
      throw native::TclError{error_text(r)};
    }
    return value_text(r.value);
  }

  [[nodiscard]] bool magic_compat() const override { return tk_.context().project().compat.magic_compat; }

  [[nodiscard]] std::function<void()> keep_edit_context() override {
    editor::EditorState* editor = tk_.context().editor();
    if (editor == nullptr) {
      return {};
    }
    return detail::keep_edit_context(*editor);
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

}  // namespace

// The editor state a `load` of another cell replaces: the current window's
// cell, view and expanded uses, the edit cell and its frame, and the box.
std::function<void()> keep_edit_context(editor::EditorState& editor) {
  view::LayoutWindow* window = editor.windows.current();
  struct Saved {
    view::WindowId id{};
    db::CellDef* root = nullptr;
    view::ViewMapping mapping;
    std::unordered_set<const db::CellUse*> expanded;
  };
  std::optional<Saved> shown;
  if (window != nullptr) {
    shown = Saved{window->id(), window->root(), window->mapping(), window->expanded_uses()};
  }
  return [&editor, shown, edit_cell = editor.edit_cell, edit_root = editor.edit_root,
          edit_transform = editor.edit_transform, box = editor.box, box_root = editor.box_root] {
    if (shown) {
      if (view::LayoutWindow* w = editor.windows.find(shown->id)) {
        if (w->root() != shown->root) {
          w->set_root(shown->root);
        }
        w->set_mapping(shown->mapping);
        w->set_expanded_uses(shown->expanded);
        w->mark_redisplay_all();
      }
    }
    editor.edit_cell = edit_cell;
    editor.edit_root = edit_root;
    editor.edit_transform = edit_transform;
    editor.box = box;
    editor.box_root = box_root;
  };
}

namespace {

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
