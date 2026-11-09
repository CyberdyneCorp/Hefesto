# CELL-PR-11: load b/inv after a/inv was loaded; getcell b/inv.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/a/inv} result; puts $result
catch {load input/b/inv} result; puts $result
catch {load top} result; puts $result
catch {getcell input/b/inv} result; puts $result
catch {cellname list allcells} result; puts $result
quit -noprompt
