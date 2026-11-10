# DRC-EDGE-1: edge poly space 2 ~poly/active ~poly/active 2 "e" (one-way); printrules; CAP on L-shapes.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 3} result; puts $result
catch {paint poly} result; puts $result
catch {box 0 0 3 10} result; puts $result
catch {paint poly} result; puts $result
catch {box 4 5 10 10} result; puts $result
catch {paint poly} result; puts $result
catch {box 20 0 30 3} result; puts $result
catch {paint poly} result; puts $result
catch {box 27 0 30 10} result; puts $result
catch {paint poly} result; puts $result
catch {box 31 5 35 10} result; puts $result
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
