# DRC-ARR-3: A 2x1 array of an asymmetric cell placed with rotation 90.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load child} result; puts $result
catch {box 0 0 10 4} result; puts $result
catch {paint m1} result; puts $result
catch {box 0 0 4 10} result; puts $result
catch {paint m1} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell child 90} result; puts $result
catch {select cell child_0} result; puts $result
catch {box 0 0 12 10} result; puts $result
catch {array 2 1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {writeall force} result; puts $result
quit -noprompt
