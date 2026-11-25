# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# hefesto_core: the platform-independent core (capability hefesto-architecture).
# Depends only on the C++20 standard library, zlib, nlohmann/json and, since
# M4, the embedded Tcl (hefesto::tcl, decision X-3; only script/tcl/ uses it).
# No windowing, GPU, OS-UI or process-spawning API may be used here.
# Owner: INTEGRATION (openspec/changes/m1-core-viewer/design.md D2). The M1
# areas add their sources in src/<area>/sources.cmake, never here (D3).

configure_file(include/hefesto/version.hpp.in
  "${CMAKE_CURRENT_BINARY_DIR}/include/hefesto/version.hpp" @ONLY)

add_library(hefesto_core STATIC
  src/version.cpp
  src/cmd/all_commands.cpp
  src/cmd/builtins.cpp
  src/cmd/command.cpp
  src/cmd/command_log.cpp
  src/cmd/coordinates.cpp
  src/cmd/dispatcher.cpp
  src/cmd/host_access.cpp
  src/cmd/keywords.cpp
  src/cmd/m7_stubs.cpp
  src/cmd/prompt.cpp
  src/cmd/registry.cpp
  src/cmd/quit_policy.cpp
  src/cmd/units.cpp
  src/cmd/value.cpp
  src/compat/compat_settings.cpp
  src/diag/codes.cpp
  src/diag/diagnostic.cpp
  src/diag/format.cpp
  src/diag/sink.cpp
  src/editor/coordinate_units.cpp
  src/editor/edit_frame.cpp
  src/editor/editor_state.cpp
  src/editor/extract_hook.cpp
  src/editor/post_command.cpp
  src/platform/file_picker.cpp
  src/platform/local_file_system.cpp
  src/platform/logger.cpp
  src/platform/memory_file_system.cpp
  src/platform/services.cpp
  src/platform/session_log.cpp
  src/platform/stack_thread.cpp
  src/platform/storage.cpp
  src/platform/task_executor.cpp
  src/platform/worker_host.cpp
  src/settings/project_settings.cpp
  src/util/libc_random.cpp
  src/util/record_order.cpp)
add_library(hefesto::core ALIAS hefesto_core)

# Areas (openspec/changes/m1-core-viewer/design.md D2/D3; M2:
# openspec/changes/m2-interactive-editing/design.md D1/D2). Each fragment
# calls target_sources(hefesto_core PRIVATE ...) for its own directory and is
# edited only by the owning workstream:
#   M1: tech/ TECH   db/ MODEL   cell/ CELL   display/ view/ VIEWER   pdk/ app/ PDK
#   M2: db/ EDIT (HIER: label/property files)   cell/ HIER   undo/ UNDO
#       select/ connect/ SELECT   ui/ DISPATCH   display/ view/ VIEW   wiring/ WIRE
#   M3 (openspec/changes/m3-drc/design.md D1/D2): drc/ INTEGRATION (shared
#       files; rules/ RULES, check/ CHECK, engine/ ENGINE, commands/ COMMANDS
#       each with its own sources.cmake)   hints/ HINTS
#   M4 (openspec/changes/m4-scripting/design.md D1/D2): script/ INTEGRATION
#       (shared files; tcl/ TCL, binding/ BINDING, pdk/ PDKINT each with its
#       own sources.cmake)   pcell/ INTEGRATION (framework/, native/ PCELL)
#       tools/ TOOLS   gui/ GUIMGR
#   M5 (openspec/changes/m5-mask-gds-cif/design.md D1/D2): mask/, gds/, cif/
#       INTEGRATION (shared files; each subdirectory has its own
#       sources.cmake owned by its workstream: mask/{plane,ops} OPS,
#       mask/style STYLES, mask/input INPUT, mask/gen GEN, gds/{stream,read}
#       GDSR, gds/{write,command} GDSW, cif/* CIF)
#   M6 (openspec/changes/m6-lef-def-oasis/design.md D1/D2): lef/, def/,
#       oasis/, extract/ INTEGRATION (shared files; each subdirectory has its
#       own sources.cmake owned by its workstream: lef/lex LEFR, lef/tech
#       LEFTECH, lef/via LEFVIA, lef/read LEFR, lef/write LEFW, lef/command
#       INTEGRATION (dispatcher), def/read DEFR, def/write DEFW, oasis/codec
#       OASFMT, oasis/read OASR, oasis/write OASW, oasis/command OASCMD,
#       extract/tech EXTSUB)   extflat/ EXTSUB
#   M8 (openspec/changes/m8-schematic/design.md D3): schem/ SCAFFOLD (shared
#       files; file/ FORMAT, sym/ lib/ tcl/ SYMLIB, net/ NETLIST, edit/ EDIT,
#       cmd/ CMD, sim/ SIMINT, each with its own sources.cmake)
#   M10 (openspec/changes/m10-3d-view/design.md D2/D3): view3d/ SCAFFOLD
#       (shared files; each subdirectory has its own sources.cmake owned by
#       its workstream: view3d/stack TECH, view3d/mesh MESH, view3d/camera
#       CAMERA, view3d/client WIND3D, view3d/{pick,section} INSPECT,
#       view3d/export EXPORT, view3d/overlay OVERLAY); extract/tech is
#       extended by TECH (the 3D height subset, design D5)
#   M7 (openspec/changes/m7-extraction/design.md D2/D3): extract/, netlist/,
#       extresist/ INTEGRATION (shared files; each subdirectory has its own
#       sources.cmake owned by its workstream: extract/tech EXTTECH,
#       extract/{node,file} EXTNODE, extract/cap EXTCAP, extract/device
#       EXTDEV, extract/hier EXTHIER, extract/{command,length} EXTCMD,
#       netlist/spice SPICE, netlist/{sim,extcheck} SIM, extresist/network
#       RESNET, extresist/{command,output} RESCMD)   extflat/ EXTFLAT
#       antenna/ ANTENNA
#   M9 (openspec/changes/m9-lvs-simulation/design.md D3): lvs/, sim/
#       SCAFFOLD (shared files; each subdirectory has its own sources.cmake
#       owned by its workstream: lvs/{net,read} LVSREAD, lvs/setup LVSSETUP,
#       lvs/cmp LVSALG, lvs/reduce LVSRED, lvs/prop LVSPROP, lvs/hier
#       LVSHIER, lvs/report LVSREP, lvs/cmd LVSCMD, sim/engine SIMENG,
#       sim/deck SIMDECK, sim/result SIMRES, sim/probe SIMPROBE, sim/cmd
#       SIMCMD, sim/runset SIMADV, sim/meas SIMMEAS)
set(HEFESTO_CORE_AREAS tech db cell display view pdk app undo select connect ui wiring drc hints
                       script pcell tools gui mask gds cif lef def oasis extflat extract schem view3d
                       netlist extresist antenna lvs sim)
foreach(_area IN LISTS HEFESTO_CORE_AREAS)
  include("${CMAKE_CURRENT_SOURCE_DIR}/src/${_area}/sources.cmake")
endforeach()

target_include_directories(hefesto_core
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_compile_features(hefesto_core PUBLIC cxx_std_20)
target_link_libraries(hefesto_core
  PRIVATE
    hefesto::zlib
    hefesto::tcl
    nlohmann_json::nlohmann_json)
find_package(Threads REQUIRED)
target_link_libraries(hefesto_core PUBLIC Threads::Threads)
hefesto_set_warnings(hefesto_core)

# Floating point (openspec/changes/m3-drc/design.md D3): no fused
# multiply-add contraction, so the DRC Euclidean filter (float projection)
# and diagonal offsets (double sqrt) round as Magic's x86_64 build and
# identically on x86_64 and aarch64. PUBLIC so that inline code compiled
# into tests and front ends follows the same rule. Never add -ffast-math.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(hefesto_core PUBLIC -ffp-contract=off)
endif()
