# DRC-NMG-1: Two cells with 45-degree m1 whose overlap intersection is off the grid (why 5).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {splitpaint nw m1} result; puts $result
catch {load b} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {splitpaint ne m1} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell a} result; puts $result
catch {box 3 0 3 0} result; puts $result
catch {getcell b} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {writeall force} result; puts $result
quit -noprompt
