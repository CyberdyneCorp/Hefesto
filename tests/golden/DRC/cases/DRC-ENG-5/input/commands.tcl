# DRC-ENG-5: A child with an internal width error far from parent paint; drc why over the copied error.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load child} result; puts $result
catch {box 0 0 2 10} result; puts $result
catch {paint m1} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell child} result; puts $result
catch {box 100 0 110 10} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {writeall force} result; puts $result
catch {box 0 0 3 10} result; puts $result
catch {drc why} result; puts $result
catch {box 1 1 2 2} result; puts $result
catch {drc why} result; puts $result
catch {box 50 0 60 10} result; puts $result
catch {drc why} result; puts $result
quit -noprompt
