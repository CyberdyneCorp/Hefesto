# CELL-RD-21: A property value of 5000 characters.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/a} result; puts $result
catch {property long} result; puts $result
catch {cellname list allcells} result; puts $result
catch {save a_out} result; puts $result
quit -noprompt
