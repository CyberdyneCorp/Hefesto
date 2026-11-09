# CELL-PR-10: x.mag and x.mag.gz with different content; locking on and off.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/x} result; puts $result
catch {save x_out} result; puts $result
catch {locking disable} result; puts $result
catch {load input/y} result; puts $result
catch {save y_out} result; puts $result
quit -noprompt
