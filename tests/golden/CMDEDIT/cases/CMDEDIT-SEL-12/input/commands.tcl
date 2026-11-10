# CMDEDIT-SEL-12: stretch e 10 of a wire end; stretch n 5 with abutting poly.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load s1} result; puts $result
catch {box 0 0 30 4} result; puts $result
catch {paint metal1} result; puts $result
catch {box 20 0 30 4} result; puts $result
catch {select area} result; puts $result
catch {stretch e 10} result; puts $result
catch {save s1} result; puts $result
catch {load s2} result; puts $result
catch {box 0 0 30 4} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 4 30 8} result; puts $result
catch {paint poly} result; puts $result
catch {box 0 0 30 4} result; puts $result
catch {select area metal1} result; puts $result
catch {stretch n 5} result; puts $result
catch {save s2} result; puts $result
quit -noprompt
