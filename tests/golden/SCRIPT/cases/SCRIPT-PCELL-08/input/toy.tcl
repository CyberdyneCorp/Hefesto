# toy: a device library of the pcell-toolkit contract (Library and device procedure contract)
namespace eval toy {}
proc toy::nfet_defaults {} { return {w 1 l 0.15 nf 1} }
proc toy::nfet_convert {parameters} { return $parameters }
proc toy::nfet_dialog {parameters} {
    magic::add_entry w "Width (um)" $parameters
    magic::add_entry l "Length (um)" $parameters
    magic::add_entry nf "Fingers" $parameters
    magic::add_dependency toy::nfet_dep nfet w l
}
proc toy::nfet_dep {parameters {target {}}} { return $parameters }
proc toy::nfet_check {parameters} { return $parameters }
proc toy::nfet_draw {parameters} {
    set w [dict get $parameters w]
    set l [dict get $parameters l]
    set snap [snap]
    snap internal
    box 0 0 [expr {int($l * 100)}] [expr {int($w * 100)}]
    paint poly
    box 0 0 0 0
    label G c poly
    snap $snap
}
proc toy::bad_defaults {} { return {w 1} }
proc toy::bad_convert {parameters} { return $parameters }
proc toy::bad_check {parameters} { return $parameters }
proc toy::bad_draw {parameters} { error "bad layer" }
proc toy::fixed_defaults {} { return {nocell 1 nx 2 ny 3 pitchx 10 pitchy 12} }
proc toy::fixed_convert {parameters} { return $parameters }
proc toy::fixed_check {parameters} { return $parameters }
proc toy::fixed_draw {parameters} {
    set nx [dict get $parameters nx]
    set ny [dict get $parameters ny]
    set use [getcell fixedcap]
    box 0 0 0 0
    array $nx $ny
    return $use
}
