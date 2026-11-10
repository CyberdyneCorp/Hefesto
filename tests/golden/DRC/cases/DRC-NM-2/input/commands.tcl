# DRC-NM-2: width m1 4 angles; a 45-degree m1 wire of perpendicular width about 2 (3/sqrt 2).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 3 3} result; puts $result
catch {splitpaint nw m1} result; puts $result
catch {box 0 3 3 6} result; puts $result
catch {splitpaint se m1} result; puts $result
catch {box 3 3 6 6} result; puts $result
catch {splitpaint nw m1} result; puts $result
catch {box 3 6 6 9} result; puts $result
catch {splitpaint se m1} result; puts $result
catch {box 6 6 9 9} result; puts $result
catch {splitpaint nw m1} result; puts $result
catch {box 6 9 9 12} result; puts $result
catch {splitpaint se m1} result; puts $result
catch {box 9 9 12 12} result; puts $result
catch {splitpaint nw m1} result; puts $result
catch {box 9 12 12 15} result; puts $result
catch {splitpaint se m1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
