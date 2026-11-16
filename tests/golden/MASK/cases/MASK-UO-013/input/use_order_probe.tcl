# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Use-order probe (m5-mask-gds-cif task 1.5, design D7; decisions Q-MASK-204 and
# Q-MASK-323). Sourced by the MASK-UO-* golden cases, which Magic 8.3.694 runs in
# the capture image (`magic -dnull -noconsole -norc input/commands.tcl`) and
# hefesto-cli replays with `--magic`. Each case drives placements, searches and
# exact-match lookups on the cell-use index of one root cell, reads the order in
# which Magic enumerates the uses back from GDS and CIF writes, and records the
# whole sequence in `trace.txt` (format: tests/data/db/use_index/README.md).
# INDEX's trace test (task 5.1) replays these traces against `db::UseIndex`.
#
# Magic primitive behind each step (Magic 8.3.694 @ fc5daa9e):
#   place    `getcell`: DBCellFindDup (BPE_EQUAL, database/DBcell.c:107), then
#            DBPlaceCell (BPAdd) (commands/CmdFI.c:1093-1099).
#   equal    `getcell` on an existing use's exact position: DBCellFindDup finds it
#            and nothing is placed ("Can't place a cell on an exact copy of itself").
#   search   `expand` with the box: one DBSrCellPlaneArea (BPE_OVERLAP) over the box
#            on the root's plane (DBExpandAll, database/DBexpand.c; commands/CmdE.c).
#   remove   `select cell <id> at <ll>` then `delete`: two BPE_OVERLAP searches over
#            the 1x1 square at the use's lower-left corner (DBSeeTypesAll,
#            commands/CmdRS.c:1380-1440; selEnumCFunc1, select/selEnum.c:650-663),
#            then BPDelete.
#   resize   paint in a child: DBReComputeBbox re-places every use of the child
#            in its cd_parents order (database/DBcellbox.c:728-764); the order is
#            recorded first with `cellname list instances`.
#   readmag  `load` of a .mag file: its uses are placed in file order
#            (database/DBio.c, DBPlaceCell); `load` makes no search.
#   gds/cif  a write. With `cif *hier write disable` and `cif *array write
#            disable`, and paint around every use so that the subtree-loading
#            search over the root's bounding box surrounds the uses
#            (calma/CalmaWrite.c:316, bplane/bpEnum.c:104-106), a write makes no
#            search that can rebuild the bins: it reads the order without changing
#            it. Otherwise the write is itself the step under test (D7).
# `cellname list childinst` walks the cell's use-id hash table
# (database/DBcellname.c:524-534), not the bplane, so its order is recorded as
# `childinst` (a check of the id hash order) and never as a use order.
#
# Every procedure lives in ::uo and none is named like a Magic command, so the
# Magic commands it calls resolve to the global ones.

namespace eval ::uo {
    variable chan ""
    variable root ""
    variable seq 0
    variable writes 0
    variable placed {}
    variable live {}
    variable defs
    variable boxes
    array set defs {}
    array set boxes {}
}

# --- trace -------------------------------------------------------------------

proc ::uo::start {caseId rootCell} {
    variable chan
    variable root
    set root $rootCell
    set chan [open trace.txt w]
    fconfigure $chan -translation lf -encoding utf-8
    emit trace 1 $caseId
    emit root $rootCell
}

proc ::uo::emit {args} {
    variable chan
    puts $chan [join $args " "]
    flush $chan
}

proc ::uo::next_seq {} {
    variable seq
    return [incr seq]
}

proc ::uo::fail {n op message} {
    emit error $n $op [string map {"\n" " " "\r" " "} [string trim $message]]
}

proc ::uo::finish {} {
    variable chan
    emit end
    close $chan
    set chan ""
}

# --- the uses the probe knows about ------------------------------------------

proc ::uo::remember {id def rect} {
    variable defs
    variable boxes
    variable placed
    variable live
    set defs($id) $def
    set boxes($id) $rect
    lappend placed $id
    lappend live $id
}

proc ::uo::use_id {k} {
    variable placed
    set id [lindex $placed $k]
    if {$id eq ""} {
        error "no use number $k"
    }
    return $id
}

proc ::uo::union {a b} {
    lassign $a ax1 ay1 ax2 ay2
    lassign $b bx1 by1 bx2 by2
    return [list [expr {min($ax1, $bx1)}] [expr {min($ay1, $by1)}] \
                 [expr {max($ax2, $bx2)}] [expr {max($ay2, $by2)}]]
}

proc ::uo::live_bbox {} {
    variable live
    variable boxes
    set result ""
    foreach id $live {
        set result [expr {$result eq "" ? $boxes($id) : [union $result $boxes($id)]}]
    }
    return $result
}

# The box in internal units (`getcell` leaves it on the new use's bounding box).
# `snap internal` prints plain integers; a units suffix would be dropped here.
proc ::uo::box_rect {} {
    return [lmap value [box values] {scan $value %d}]
}

# --- steps -------------------------------------------------------------------

# Place a use of `def` with its lower-left corner at (x, y).
proc ::uo::place {def x y} {
    set n [next_seq]
    if {[catch {
        box $x $y $x $y
        set id [getcell $def]
        set rect [box_rect]
    } message]} {
        fail $n place $message
        return
    }
    if {$id eq ""} {
        fail $n place "getcell $def at $x $y returned no use id"
        return
    }
    remember $id $def $rect
    emit place $n $id $def {*}$rect
}

# Try to place a copy of use number k on top of itself: an exact-match lookup.
proc ::uo::equal {k} {
    variable defs
    variable boxes
    set n [next_seq]
    if {[catch {
        set id [use_id $k]
        lassign $boxes($id) x y
        box $x $y $x $y
        set copy [getcell $defs($id)]
    } message]} {
        fail $n equal $message
        return
    }
    if {$copy eq ""} {
        emit equal $n $id $defs($id) {*}$boxes($id) rejected
        return
    }
    set rect [box_rect]
    remember $copy $defs($id) $rect
    emit equal $n $id $defs($id) {*}$boxes($id) placed $copy {*}$rect
}

# One area search of the root's use index over the box (x1, y1)-(x2, y2).
proc ::uo::search {x1 y1 x2 y2} {
    set n [next_seq]
    if {[catch {
        select clear
        box $x1 $y1 $x2 $y2
        expand
    } message]} {
        fail $n search $message
        return
    }
    emit search $n $x1 $y1 $x2 $y2
}

# A search over the bounding box of the live uses grown by `d` on every side.
proc ::uo::search_around {d} {
    lassign [live_bbox] x1 y1 x2 y2
    search [expr {$x1 - $d}] [expr {$y1 - $d}] [expr {$x2 + $d}] [expr {$y2 + $d}]
}

# Delete use number k.
proc ::uo::remove {k} {
    variable boxes
    variable live
    set n [next_seq]
    if {[catch {
        set id [use_id $k]
        lassign $boxes($id) x y
        select cell $id at ${x}i ${y}i
        delete
    } message]} {
        fail $n remove $message
        return
    }
    set live [lsearch -all -inline -not -exact $live $id]
    emit remove $n $id {*}$boxes($id)
}

# Paint `layer` over (x1, y1)-(x2, y2) of child `def`, in the child's coordinates,
# which grows the child's bounding box: every use of it is re-placed.
proc ::uo::resize {def layer x1 y1 x2 y2} {
    variable root
    set n [next_seq]
    if {[catch {
        set parents [cellname list instances $def]
        select clear
        load $def
        box $x1 $y1 $x2 $y2
        paint $layer
        load $root
    } message]} {
        fail $n resize $message
        return
    }
    emit instances $n $def {*}$parents
    emit resize $n $def $x1 $y1 $x2 $y2
    grow_uses $n $def [list $x1 $y1 $x2 $y2]
}

# The new bounding box of every live use of `def` (identity transforms only).
proc ::uo::grow_uses {n def rect} {
    variable live
    variable defs
    variable boxes
    lassign $rect x1 y1 x2 y2
    foreach id $live {
        if {$defs($id) ne $def} {
            continue
        }
        lassign $boxes($id) ux uy
        set moved [list [expr {$ux + $x1}] [expr {$uy + $y1}] [expr {$ux + $x2}] [expr {$uy + $y2}]]
        set boxes($id) [union $boxes($id) $moved]
        emit bbox $n $id {*}$boxes($id)
    }
}

# Load the root cell from its .mag file, recording the uses in file order.
proc ::uo::readmag {path} {
    variable root
    set n [next_seq]
    if {[catch {set uses [mag_uses $path]} message]} {
        fail $n readmag $message
        return
    }
    emit readmag $n $root $path
    foreach {id def rect} $uses {
        remember $id $def $rect
        emit place [next_seq] $id $def {*}$rect mag
    }
    if {[catch {load $root} message]} {
        fail [next_seq] readmag $message
    }
}

# {id def rect ...} of the `use` blocks of a .mag file (translations only).
proc ::uo::mag_uses {path} {
    set f [open $path r]
    set lines [split [read $f] "\n"]
    close $f
    set uses {}
    foreach line $lines {
        set words [regexp -all -inline {\S+} $line]
        switch -- [lindex $words 0] {
            use       {lassign $words - def id}
            transform {lassign $words - a b c d e f}
            box       {lappend uses $id $def [mag_box $words $a $b $c $d $e $f]}
        }
    }
    return $uses
}

proc ::uo::mag_box {words a b c d e f} {
    if {$a != 1 || $b != 0 || $d != 0 || $e != 1} {
        error "use transform $a $b $c $d $e $f is not a translation"
    }
    lassign $words - x1 y1 x2 y2
    return [list [expr {$x1 + $c}] [expr {$y1 + $f}] [expr {$x2 + $c}] [expr {$y2 + $f}]]
}

# --- readouts ----------------------------------------------------------------

# The use-id hash order of the root (not a bplane order).
proc ::uo::childinst {} {
    variable root
    set n [next_seq]
    if {[catch {cellname list childinst $root} ids]} {
        fail $n childinst $ids
        return
    }
    emit childinst $n $root {*}$ids
}

proc ::uo::written_file {name ext} {
    foreach candidate [list $name.$ext $name] {
        if {[file isfile $candidate]} {
            return $candidate
        }
    }
    error "no file written for $name.$ext"
}

proc ::uo::read_binary {path} {
    set f [open $path rb]
    set data [read $f]
    close $f
    return $data
}

# `gds write`, then the structure order and each structure's SREF/AREF order.
proc ::uo::gds_order {} {
    variable writes
    set n [next_seq]
    set name uo_[incr writes]
    if {[catch {
        gds write $name
        set path [written_file $name gds]
        set structures [gds_structures [gds_records [read_binary $path]]]
    } message]} {
        fail $n gds $message
        return
    }
    emit write $n gds $path
    emit structures $n gds {*}[dict keys $structures]
    dict for {cell refs} $structures {
        if {[llength $refs]} {
            emit order $n gds $cell {*}$refs
        }
    }
}

# `cif write`, then the symbol numbering and each symbol's call order.
proc ::uo::cif_order {} {
    variable writes
    set n [next_seq]
    set name uo_[incr writes]
    if {[catch {
        cif write $name
        set path [written_file $name cif]
        set symbols [cif_symbols [read_binary $path]]
    } message]} {
        fail $n cif $message
        return
    }
    emit write $n cif $path
    set numbers {}
    foreach {number cell calls} $symbols {
        lappend numbers $number:$cell
    }
    emit symbols $n cif {*}$numbers
    foreach {number cell calls} $symbols {
        if {[llength $calls]} {
            emit order $n cif $cell {*}$calls
        }
    }
}

# The orders a write shows: the hash order, then the GDS order.
proc ::uo::readout {} {
    childinst
    gds_order
}

# --- GDS and CIF decoding ----------------------------------------------------

# {rectype body ...} of a GDS stream, up to ENDLIB.
proc ::uo::gds_records {data} {
    set records {}
    set offset 0
    set size [string length $data]
    while {$offset + 4 <= $size} {
        binary scan $data @${offset}Sucu length rectype
        if {$length < 4 || $offset + $length > $size} {
            error "bad GDS record at byte $offset"
        }
        lappend records $rectype [string range $data [expr {$offset + 4}] [expr {$offset + $length - 1}]]
        incr offset $length
        if {$rectype == 4} {
            break
        }
    }
    return $records
}

proc ::uo::gds_string {body} {
    return [string trimright $body "\0"]
}

# Dict structure name -> list of its references, in file order. A reference is
# named by its use id (PROPATTR 61, which Magic always writes), else
# <structure>@<x>,<y>.
proc ::uo::gds_structures {records} {
    set state [dict create structures {} name "" refs {} inref 0 sname "" xy "" attr 0 id ""]
    foreach {rectype body} $records {
        set state [gds_step $state $rectype $body]
    }
    return [dict get $state structures]
}

proc ::uo::gds_step {state rectype body} {
    switch -- $rectype {
        6  {dict set state name [gds_string $body]; dict set state refs {}}
        7  {dict set state structures [dict get $state name] [dict get $state refs]}
        10 - 11 {set state [dict merge $state {inref 1 sname "" xy "" attr 0 id ""}]}
        default {
            if {[dict get $state inref]} {
                set state [gds_ref_step $state $rectype $body]
            }
        }
    }
    return $state
}

proc ::uo::gds_ref_step {state rectype body} {
    switch -- $rectype {
        18 {dict set state sname [gds_string $body]}
        16 {binary scan $body II x y; dict set state xy $x,$y}
        43 {binary scan $body S attr; dict set state attr $attr}
        44 {if {[dict get $state attr] == 61} {dict set state id [gds_string $body]}}
        17 {
            dict lappend state refs [ref_name [dict get $state id] [dict get $state sname] [dict get $state xy]]
            dict set state inref 0
        }
    }
    return $state
}

proc ::uo::ref_name {id sname xy} {
    return [expr {$id ne "" ? $id : "$sname@$xy"}]
}

# {number name calls ...} of the symbols of a CIF file, in definition order. A
# call is named by the use id of its `91` line, else #<symbol number>. Comments
# (statements starting with `(`) are skipped.
proc ::uo::cif_symbols {text} {
    set state [dict create symbols {} number "" name "" calls {} id ""]
    foreach statement [split $text ";"] {
        set state [cif_step $state [string trim $statement]]
    }
    return [dict get $state symbols]
}

proc ::uo::cif_step {state statement} {
    if {[string index $statement 0] eq "("} {
        return $state
    }
    if {[regexp {^DS\s*(\d+)} $statement -> number]} {
        return [dict merge $state [list number $number name "" calls {} id ""]]
    }
    if {[regexp {^9\s+(\S+)} $statement -> name]} {
        dict set state name $name
    } elseif {[regexp {^91\s+(\S+)} $statement -> id]} {
        dict set state id $id
    } elseif {[regexp {^C\s*(\d+)} $statement -> callee]} {
        dict lappend state calls [expr {[dict get $state id] ne "" ? [dict get $state id] : "#$callee"}]
        dict set state id ""
    } elseif {[regexp {^DF} $statement]} {
        dict lappend state symbols [dict get $state number] [dict get $state name] [dict get $state calls]
    }
    return $state
}
