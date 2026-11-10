# DRC-WS-RL-H: DRC-WS-RL-V rotated 90 degrees (Q-DRC-7).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 40 12} result; puts $result
catch {paint m1} result; puts $result
catch {box 0 14 25 16} result; puts $result
catch {paint m1} result; puts $result
catch {box 0 100 40 112} result; puts $result
catch {paint m1} result; puts $result
catch {box 0 114 15 116} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
