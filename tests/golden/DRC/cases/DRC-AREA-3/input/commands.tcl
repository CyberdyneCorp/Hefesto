# DRC-AREA-3: area m1,m1c 20 4; regions of m1 and m1c whose tiles start right of another set type (Q-DRC-21).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 3 3} result; puts $result
catch {paint m1c} result; puts $result
catch {box 3 0 5 3} result; puts $result
catch {paint m1} result; puts $result
catch {box 10 0 12 4} result; puts $result
catch {paint m1c} result; puts $result
catch {box 12 0 14 4} result; puts $result
catch {paint m1} result; puts $result
catch {box 20 0 22 6} result; puts $result
catch {paint m1c} result; puts $result
catch {box 22 0 24 6} result; puts $result
catch {paint m1} result; puts $result
catch {box 30 0 32 6} result; puts $result
catch {paint m1} result; puts $result
catch {box 32 0 34 6} result; puts $result
catch {paint m1c} result; puts $result
catch {box 34 2 36 4} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
