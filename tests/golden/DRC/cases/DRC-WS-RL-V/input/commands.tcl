# DRC-WS-RL-V: widespacing m1 10 20 m1 4 touching_ok (runlength 20); vertical parallel runs 25 and 15 (Q-DRC-7).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 12 40} result; puts $result
catch {paint m1} result; puts $result
catch {box 14 0 16 25} result; puts $result
catch {paint m1} result; puts $result
catch {box 100 0 112 40} result; puts $result
catch {paint m1} result; puts $result
catch {box 114 0 116 15} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
