# A rev-0 library (toolkit_rev0.tcl): <lib>::<dev>_defaults, _draw
namespace eval old {}
proc old::dev_defaults {} { return {w 1 l 1} }
proc old::dev_convert {parameters} { return $parameters }
proc old::dev_check {parameters} { return $parameters }
proc old::dev_draw {parameters} {
    set w [dict get $parameters w]
    box 0 0 [expr {int($w * 10)}] 10
    paint m1
}
