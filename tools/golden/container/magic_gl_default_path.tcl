# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Build-time probe of the magic-gl image variant (Containerfile, WITH_OPENGL=1), informational
# only: `specialopen wind3d` with the default Tk path `.magic3d` (no toplevel). The first
# dispatch of the MISC-3D-* cases saw Magic crash here; the Containerfile runs this under gdb
# and prints the backtrace without failing the build.
if {[catch {
    tech load scmos -noprompt
    load probe
    box 0 0 10 10
    paint m1
    specialopen wind3d probe
    .magic3d view 0 10 0 rel
} error]} {
    puts [list magic-gl-default-error $error]
}
set code [catch {.magic3d view} result]
puts [list magic-gl-default $code $result]
flush stdout
quit -noprompt
