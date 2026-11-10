# DRC-XOV-1: exact_overlap m1c; sibling cells with m1c [0,4]^2 and [2,6]x[0,4]; the same cells abutting at x=24; an m1c cell over the m1 of a third cell.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint m1c} result; puts $result
catch {load b} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint m1c} result; puts $result
catch {load c} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint m1} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell a} result; puts $result
catch {box 2 0 2 0} result; puts $result
catch {getcell b} result; puts $result
catch {box 20 0 20 0} result; puts $result
catch {getcell a} result; puts $result
catch {box 24 0 24 0} result; puts $result
catch {getcell b} result; puts $result
catch {box 40 0 40 0} result; puts $result
catch {getcell c} result; puts $result
catch {box 42 2 42 2} result; puts $result
catch {getcell a} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {writeall force} result; puts $result
quit -noprompt
