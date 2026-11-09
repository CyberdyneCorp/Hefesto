# CELL-RD-16: Self use (a uses a); mutual use b and c.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/a} result; puts $result
catch {load input/b} result; puts $result
catch {cellname list allcells} result; puts $result
quit -noprompt
