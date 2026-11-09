# CELL-RD-18: Port records with 2..5 tokens, a port before any label, a second port, unknown use/class/shape.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/a} result; puts $result
catch {cellname list allcells} result; puts $result
catch {save a_out} result; puts $result
quit -noprompt
