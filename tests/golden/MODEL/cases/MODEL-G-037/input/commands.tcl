# MODEL-G-037: cellname delete inv while instantiated; then after deleting the parent.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load inv} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save inv} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell inv} result; puts $result
catch {save top} result; puts $result
catch {load other} result; puts $result
catch {cellname delete inv} result; puts $result
catch {cellname delete top} result; puts $result
catch {cellname delete inv} result; puts $result
catch {cellname list allcells} result; puts $result
quit -noprompt
