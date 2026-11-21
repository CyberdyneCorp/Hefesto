# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Build-time smoke test of the magic-gl image variant (Containerfile, WITH_OPENGL=1):
# the 3D window opens under Xvfb + Mesa, redraws after a relative rotation and answers `view`.
# It opens the window the way Magic's own wrapper does (magic::render3d, tcltk/wrapper.tcl):
# a mapped toplevel and an explicit Tk path. Every step is caught so that Magic's Tk wrapper
# always reaches `quit` (no hang on an error).
if {[catch {
    tech load scmos -noprompt
    load smoke
    box 0 0 10 10
    paint m1
    toplevel .render
    tkwait visibility .render
    specialopen wind3d smoke .render.magic
    .render.magic view 0 10 0 rel
} error]} {
    puts [list magic-gl-smoke-error $error]
}
set code [catch {.render.magic view} result]
puts [list magic-gl-smoke $code $result]
flush stdout
quit -noprompt
