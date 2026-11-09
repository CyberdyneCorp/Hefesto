# CELL-PR-09: Bad recorded path with the cell in an addpath directory; then load -dereference.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {addpath input/lib} result; puts $result
catch {load input/top} result; puts $result
catch {cellname list allcells} result; puts $result
catch {load input/top2 -dereference} result; puts $result
catch {cellname list allcells} result; puts $result
catch {save top2_out} result; puts $result
quit -noprompt
