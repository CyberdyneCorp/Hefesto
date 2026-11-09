# CELL-RD-15: Illegal transform 2 0 0 0 1 0; malformed array line.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/a} result; puts $result
catch {cellname list allcells} result; puts $result
quit -noprompt
