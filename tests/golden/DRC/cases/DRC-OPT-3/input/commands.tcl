# DRC-OPT-3: edge4way space m1 2 m1 m1 2 "A" then edge space m1 3 m1 m1 3 "B" (Q-DRC-5); L-shaped m1 and notches.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 1} result; puts $result
catch {paint m1} result; puts $result
catch {box 0 0 1 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 20 0 22 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 20 8 30 10} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
catch {drc printrules rules.txt} result; puts $result
quit -noprompt
