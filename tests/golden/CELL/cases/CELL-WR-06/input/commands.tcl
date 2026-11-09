# CELL-WR-06: Instance ids a10, B1, a2, x, x2, A1, a1: use order on save.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/a} result; puts $result
catch {save a_out} result; puts $result
quit -noprompt
