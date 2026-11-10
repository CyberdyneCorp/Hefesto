# DRC-OG-1: off_grid m1 5; m1 [3,10]x[0,10]; m1 [-8,-3]x[-7,2] (Q-DRC-9).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 3 0 10 10} result; puts $result
catch {paint m1} result; puts $result
catch {box -8 -7 -3 2} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
