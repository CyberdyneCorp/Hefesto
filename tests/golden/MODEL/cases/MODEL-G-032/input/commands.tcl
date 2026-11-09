# MODEL-G-032: instance lock inv_0; save (move/delete not authored).
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
catch {instance lock inv_0} result; puts $result
catch {save top} result; puts $result
catch {cellname list modified} result; puts $result
quit -noprompt
