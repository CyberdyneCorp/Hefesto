// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A `wind3d` test session on a technology with a contact (`via` between
// metal1 and metal2), for the J1 option "Span connected layers" as window
// state (client and export tests). Owner: CORE (round 6).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <string_view>

#include "client_fixture.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/view3d/client/scene.hpp"

namespace hefesto::view3d::test {

// The MISC-3D technology with a contact `via` between metal1 and metal2:
// metal1 is drawn from 0.5 to 0.8, metal2 from 1.0 to 1.3 and the via
// (its own `height` line) from 0.6 to 1.0.
inline constexpr std::string_view kViaTech = R"(tech
    format 35
    v3dvia
end
planes
    metal1,m1
    metal2,m2
end
types
    metal1  metal1,m1
    metal2  metal2,m2
    metal1  via
end
styles
    styletype mos
    metal1 20
    metal2 21
    via 22
end
contact
    via metal1 metal2
end
compose
end
connect
    metal1,via  metal1,via
    metal2,via  metal2,via
end
cifoutput
style gdsii
    scalefactor 10
    layer MET1 m1,via
        calma 68 20
end
cifinput
end
drc
end
extract
style default
    lambda 1
    step 100
    sidehalo 0
    planeorder metal1 0
    planeorder metal2 1
    height metal1 0.5 0.3
    height metal2 1.0 0.3
    height via 0.6 0.4
end
)";

// A session on kViaTech: metal1 and metal2 over (0,0)-(100,50), a via at
// (20,10)-(40,30), the display bindings of the three types, and the 3D
// window open on c3d in layout-layer mode.
struct ViaSession : ClientSession {
  explicit ViaSession(bool magic_compat) : ClientSession(magic_compat, kViaTech) {
    paint("metal2", db::Rect{0, 0, 100, 50});
    paint("via", db::Rect{20, 10, 40, 30});
    editor.styles.emplace();
    auto& types = editor.styles->types.styles_of_type;
    types.resize(tech::kMaxTileTypes);
    types[*editor.technology->find_type("metal1").id] = {display::kFixedStyleCount + 20};
    types[*editor.technology->find_type("metal2").id] = {display::kFixedStyleCount + 21};
    types[*editor.technology->find_type("via").id] = {display::kFixedStyleCount + 22};
    ok("specialopen wind3d c3d");
    w3d("cif");
    REQUIRE(window().mode == Mode::layout_layer);
  }

  [[nodiscard]] LayerId layer(std::string_view type_name) {
    const auto type = editor.technology->find_type(type_name).id;
    const LayerStack stack = layout_stack(editor, project.compat.magic_compat);
    for (std::size_t i = 0; i < stack.size(); ++i) {
      if (stack.layers()[i].type == type) {
        return static_cast<LayerId>(i);
      }
    }
    FAIL("no layer of type " << type_name);
    return 0;
  }

  [[nodiscard]] Scene scene() {
    BuildContext ctx;
    return build_scene(editor, window(), project.compat.magic_compat, ctx);
  }
};

}  // namespace hefesto::view3d::test
