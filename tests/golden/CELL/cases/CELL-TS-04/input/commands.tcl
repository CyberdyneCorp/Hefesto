# CELL-TS-04: A file with timestamp 0.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/z} result; puts $result
catch {cellname list allcells} result; puts $result
quit -noprompt
