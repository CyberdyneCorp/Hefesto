# CELL-WR-05: fake: m3c2, and a split tile m2c / m3c2; save.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/fake -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint m3c2} result; puts $result
catch {box 10 0 20 10} result; puts $result
catch {splitpaint nw m2c} result; puts $result
catch {box 10 0 20 10} result; puts $result
catch {splitpaint se m3c2} result; puts $result
catch {save a} result; puts $result
quit -noprompt
