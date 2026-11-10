# DRC-CNT-1: A top cell with 3 error tiles and a child with 1: drc count [total] in every list mode; then drc on.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load child} result; puts $result
catch {box 0 0 2 10} result; puts $result
catch {paint m1} result; puts $result
catch {load top} result; puts $result
catch {box 100 0 100 0} result; puts $result
catch {getcell child} result; puts $result
catch {box 0 0 2 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 20 0 22 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 40 0 42 10} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {writeall force} result; puts $result
catch {select top cell} result; puts $result
catch {drc count} result; puts $result
catch {drc count total} result; puts $result
catch {drc list count} result; puts $result
catch {drc listall count} result; puts $result
catch {drc list count total} result; puts $result
catch {drc listall count total} result; puts $result
catch {drc on} result; puts $result
catch {drc count} result; puts $result
catch {drc count total} result; puts $result
quit -noprompt
