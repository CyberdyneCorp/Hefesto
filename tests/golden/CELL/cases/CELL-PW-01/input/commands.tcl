# CELL-PW-01: Child in a subdirectory of the parent directory; save the parent.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell input/lib/c} result; puts $result
catch {save top} result; puts $result
quit -noprompt
