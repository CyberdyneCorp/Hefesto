# CELL-WR-08: Persistent elements (rectangle, line with flags, text); load and save.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/a} result; puts $result
catch {save a_out} result; puts $result
quit -noprompt
