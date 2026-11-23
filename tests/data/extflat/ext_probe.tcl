# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# The .ext probe (openspec/changes/archive/2026-11-22-m6-lef-def-oasis task 1.6, Open Question 1 (a)).
#
# Magic writes the .ext files that the DEF-write net cases and EXTSUB's read tests take as
# inputs, since Hefesto cannot extract before M7. Run by tools/golden/ext_inputs.py in the
# pinned image (golden-capture.yml, mode ext-inputs) as
#
#     magic -dnull -noconsole -norc run.tcl
#
# from a work directory holding input/ (the layout and its technology) and this file;
# run.tcl (written by tools/golden/authoring/lefdef_cases.py) sources it and calls
# extprobe::run. Every .ext file goes to ext/ ("extract path ext"), so none lands beside
# the inputs. Each line the probe prints starts with "extprobe:"; ext_inputs.py reads them.

namespace eval extprobe {}

# extprobe::run TECH ROOT: load input/TECH.tech and the cell ROOT, then "extract all".
proc extprobe::run {tech root} {
    addpath input
    tech load input/$tech -noprompt
    drc off
    load $root
    # extract finds its cell from the box when there is no window (-dnull).
    box 0 0 1 1
    file mkdir ext
    extract path ext
    set code [catch {extract all} message]
    puts "extprobe: extract all $code [list $message]"
    puts "extprobe: feedback [feedback count]"
    foreach path [lsort [glob -nocomplain -directory ext *.ext]] {
        puts "extprobe: wrote [file tail $path] [file size $path]"
    }
    puts "extprobe: done"
    flush stdout
}
