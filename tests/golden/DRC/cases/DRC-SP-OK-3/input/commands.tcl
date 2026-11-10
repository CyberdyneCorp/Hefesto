# DRC-SP-OK-3: spacing poly pc 2 touching_ok (S1 != S2, reverse rules); printrules; CAP.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint poly} result; puts $result
catch {box 11 0 15 4} result; puts $result
catch {paint pc} result; puts $result
catch {box 10 6 14 10} result; puts $result
catch {paint pc} result; puts $result
catch {box 30 0 34 4} result; puts $result
catch {paint pc} result; puts $result
catch {box 35 0 40 10} result; puts $result
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
