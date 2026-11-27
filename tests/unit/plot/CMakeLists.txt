# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the plot area (openspec/changes/m10-plot-route/design.md D3).
# Owner: SCAFFOLD (this file and the shared tests here). Each subdirectory is
# owned by its workstream and builds its own executable once it has a
# *_test.cpp:
#   tech/    PLOTTECH hefesto_plot_tech_tests   (data: tests/data/plot/tech)
#   params/  PLOTCMD  hefesto_plot_params_tests   (data: tests/data/plot/params)
#   cmd/     PLOTCMD  hefesto_plot_cmd_tests   (data: tests/data/plot/cmd)
#   io/      PLOTCMD  hefesto_plot_io_tests   (data: tests/data/plot/io)
#   ps/      PS       hefesto_plot_ps_tests   (data: tests/data/plot/ps)
#   pnm/     PNM      hefesto_plot_pnm_tests   (data: tests/data/plot/pnm)

hefesto_add_area_tests(plot)

foreach(_plot_sub IN ITEMS tech params cmd io ps pnm)
  add_subdirectory(${_plot_sub})
endforeach()
