# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Build-time smoke test of the magic-cairo image variant (Containerfile, WITH_CAIRO=1;
# openspec/changes/m10-plot-route design D19, Q-MISC-52 (a)): Magic's Tk wrapper on the Cairo display
# (-d XR) under Xvfb writes `plot svg` for its layout window. Every step is caught so that the wrapper
# always reaches `quit` (no hang on an error).
if {[catch {
    tech load scmos -noprompt
    load smoke
    box 0 0 10 10
    paint m1
    set w [lindex [windownames layout] 0]
    if {![winfo viewable $w]} {tkwait visibility $w}
    update
    plot svg /tmp/magic_cairo_smoke
} error]} {
    puts [list magic-cairo-smoke-error $error]
}
puts [list magic-cairo-smoke [file exists /tmp/magic_cairo_smoke.svg]]
flush stdout
quit -noprompt
