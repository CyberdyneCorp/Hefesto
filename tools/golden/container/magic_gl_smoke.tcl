# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Build-time smoke test of the magic-gl image variant (Containerfile, WITH_OPENGL=1):
# the 3D window opens under Xvfb + Mesa, redraws after a relative rotation and answers `view`.
# Every step is caught so that Magic's Tk wrapper always reaches `quit` (no hang on an error).
if {[catch {
    tech load scmos -noprompt
    load smoke
    box 0 0 10 10
    paint m1
    specialopen wind3d smoke
    .magic3d view 0 10 0 rel
} error]} {
    puts [list magic-gl-smoke-error $error]
}
set code [catch {.magic3d view} result]
puts [list magic-gl-smoke $code $result]
flush stdout
quit -noprompt
