# DRC-SHOWINT-1: drc *showint 10 on a flat and on a hierarchical layout; the box after each.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load flat} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 12 0 22 10} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {drc *showint 10} result; puts $result
catch {box values} result; puts $result
catch {load child} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint m1} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell child} result; puts $result
catch {box 15 0 25 10} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {drc *showint 10} result; puts $result
catch {box values} result; puts $result
catch {box 0 0 25 10} result; puts $result
catch {drc *showint 3} result; puts $result
catch {box values} result; puts $result
catch {box 40 40 50 50} result; puts $result
catch {drc *showint 3} result; puts $result
catch {box values} result; puts $result
quit -noprompt
