# CMDEDIT-SEL-15: Instance inv 0 0 10 20, box 0 0 12 25; array 3 2; array -list count; array 0 2 3 1 (Q-CMDEDIT-63).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load inv} result; puts $result
catch {box 0 0 10 20} result; puts $result
catch {paint metal1} result; puts $result
catch {save inv} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell inv} result; puts $result
catch {select cell inv_0} result; puts $result
catch {box 0 0 12 25} result; puts $result
catch {array 3 2} result; puts $result
catch {array -list count} result; puts $result
catch {array -list} result; puts $result
catch {array 0 2 3 1} result; puts $result
catch {array -list} result; puts $result
catch {save a} result; puts $result
quit -noprompt
