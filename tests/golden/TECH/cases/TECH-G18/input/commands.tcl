# TECH-G18: stackable with no arguments: stacked types saved in a .mag; pm12c lookup.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint pc} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint m2c} result; puts $result
catch {box 10 0 14 4} result; puts $result
catch {paint ndc} result; puts $result
catch {box 10 0 14 4} result; puts $result
catch {paint m2c} result; puts $result
catch {save a} result; puts $result
catch {tech layers pm12c} result; puts $result
catch {tech layers *} result; puts $result
quit -noprompt
