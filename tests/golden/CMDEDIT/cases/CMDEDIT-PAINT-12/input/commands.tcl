# CMDEDIT-PAINT-12: A triangle, a pentagon, odd coordinates, too few coordinates and an odd coordinate count (Q-CMDEDIT-19).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load p1} result; puts $result
catch {polygon metal1 0 0 10 0 0 10} result; puts $result
catch {save p1} result; puts $result
catch {load p2} result; puts $result
catch {polygon metal1 0 0 20 0 20 10 10 20 0 10} result; puts $result
catch {save p2} result; puts $result
catch {load p3} result; puts $result
catch {polygon metal1 1 1 11 1 1 11} result; puts $result
catch {polygon metal1 0 0 10 0} result; puts $result
catch {polygon metal1 0 0 10 0 5} result; puts $result
catch {save p3} result; puts $result
quit -noprompt
