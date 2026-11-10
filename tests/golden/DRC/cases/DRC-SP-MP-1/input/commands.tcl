# DRC-SP-MP-1: spacing ndiff,m1 poly,m2 3 touching_illegal (both lists over two planes, Q-DRC-3); printrules, CAP.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint ndiff} result; puts $result
catch {box 12 0 20 10} result; puts $result
catch {paint poly} result; puts $result
catch {box 0 20 10 30} result; puts $result
catch {paint m1} result; puts $result
catch {box 12 20 20 30} result; puts $result
catch {paint m2} result; puts $result
catch {box 30 0 40 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 42 0 50 10} result; puts $result
catch {paint poly} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
catch {drc printrules rules.txt} result; puts $result
quit -noprompt
