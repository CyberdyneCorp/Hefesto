# CELL-PR-08: A missing child referenced by two parents; expand twice.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/p1} result; puts $result
catch {expand} result; puts $result
catch {load input/p2} result; puts $result
catch {expand} result; puts $result
catch {expand} result; puts $result
catch {cellname list allcells} result; puts $result
quit -noprompt
