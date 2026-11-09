# CELL-WR-02: scalegrid 1 2; all-even cell saved; then an odd rect.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {scalegrid 1 2} result; puts $result
catch {box 0 0 4 8} result; puts $result
catch {paint metal1} result; puts $result
catch {save a1} result; puts $result
catch {box 10 0 13 5} result; puts $result
catch {paint metal1} result; puts $result
catch {save a2} result; puts $result
quit -noprompt
