# DRC-PR-1: sky130A: drc printrules into a file (compared by digest: 26 MB).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/sky130A -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {scalegrid 1 2} result; puts $result
catch {drc off} result; puts $result
catch {drc euclidean on} result; puts $result
catch {snap internal} result; puts $result
catch {drc printrules rules.txt} result; puts $result
quit -noprompt
