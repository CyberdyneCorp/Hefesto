# DRC-W-2: width poly 2 "w"; poly L-shape with a 1-wide step at the inner corner (corner extension).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 3} result; puts $result
catch {paint poly} result; puts $result
catch {box 0 0 3 10} result; puts $result
catch {paint poly} result; puts $result
catch {box 3 3 4 4} result; puts $result
catch {paint poly} result; puts $result
catch {box 20 0 30 3} result; puts $result
catch {paint poly} result; puts $result
catch {box 20 4 30 7} result; puts $result
catch {paint poly} result; puts $result
catch {box 20 0 23 7} result; puts $result
catch {paint poly} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
