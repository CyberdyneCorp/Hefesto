# DRC-RO-CELL-1: A vendor-GDS child (GDS_FILE property, read from input/) and a non-editable child, both with violations; modified flags after catchup.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load input/vendor} result; puts $result
catch {load input/locked} result; puts $result
catch {cellname writeable locked false} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell vendor} result; puts $result
catch {box 50 0 50 0} result; puts $result
catch {getcell locked} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {cellname list modified} result; puts $result
catch {cellname flags vendor} result; puts $result
catch {cellname flags locked} result; puts $result
catch {save top} result; puts $result
quit -noprompt
