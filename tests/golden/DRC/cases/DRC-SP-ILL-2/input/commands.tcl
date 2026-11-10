# DRC-SP-ILL-2: spacing m1c via 2 touching_illegal across planes (coincidence rule); coincident, 1 and 4 apart.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint m1c} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint via} result; puts $result
catch {box 20 0 30 10} result; puts $result
catch {paint m1c} result; puts $result
catch {box 31 0 35 10} result; puts $result
catch {paint via} result; puts $result
catch {box 50 0 60 10} result; puts $result
catch {paint m1c} result; puts $result
catch {box 64 0 70 10} result; puts $result
catch {paint via} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
catch {drc printrules rules.txt} result; puts $result
quit -noprompt
