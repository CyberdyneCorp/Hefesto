# TECH-G27: tech locked pc; erase metal1 over a pc tile.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {tech locked pc} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint pc} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {erase metal1} result; puts $result
catch {save a} result; puts $result
quit -noprompt
