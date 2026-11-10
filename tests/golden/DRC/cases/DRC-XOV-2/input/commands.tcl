# DRC-XOV-2: exact_overlap m1c; contacts exactly coincident in two cells.
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
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell a} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell b} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {writeall force} result; puts $result
quit -noprompt
