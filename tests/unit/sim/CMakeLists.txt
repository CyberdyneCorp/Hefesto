# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the sim area (openspec/changes/m9-lvs-simulation/design.md D3).
# Owner: SCAFFOLD (this file and the shared tests here). Each subdirectory
# is owned by its workstream and builds its own executable once it has a
# *_test.cpp:
#   engine/  SIMENG    hefesto_sim_engine_tests   (data: tests/data/sim/engine)
#   deck/    SIMDECK   hefesto_sim_deck_tests   (data: tests/data/sim/deck)
#   result/  SIMRES    hefesto_sim_result_tests   (data: tests/data/sim/result)
#   probe/   SIMPROBE  hefesto_sim_probe_tests   (data: tests/data/sim/probe)
#   cmd/     SIMCMD    hefesto_sim_cmd_tests   (data: tests/data/sim/cmd)
#   runset/  SIMADV    hefesto_sim_runset_tests   (data: tests/data/sim/runset)
#   meas/    SIMMEAS   hefesto_sim_meas_tests   (data: tests/data/sim/meas)

hefesto_add_area_tests(sim)

foreach(_sim_sub IN ITEMS engine deck result probe cmd runset meas)
  add_subdirectory(${_sim_sub})
endforeach()
