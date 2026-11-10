# CMDEDIT-PAINT-03: metal1+poly at 5 5 6 6; paint pick 5 5; paint pick 50 50.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 5 5 6 6} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 6 6} result; puts $result
catch {paint poly} result; puts $result
catch {box 20 0 30 10} result; puts $result
catch {paint pick 5 5} result; puts $result
catch {save p1} result; puts $result
catch {paint pick 50 50} result; puts $result
catch {save p2} result; puts $result
quit -noprompt
