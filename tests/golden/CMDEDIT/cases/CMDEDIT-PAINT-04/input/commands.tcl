# CMDEDIT-PAINT-04: metal1, poly and label A in the box: erase; erase poly; erase labels; erase space.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load e1} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint poly} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label A} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {erase} result; puts $result
catch {save e1} result; puts $result
catch {load e2} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint poly} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label A} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {erase poly} result; puts $result
catch {save e2} result; puts $result
catch {load e3} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint poly} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label A} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {erase labels} result; puts $result
catch {save e3} result; puts $result
catch {load e4} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint poly} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label A} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {erase space} result; puts $result
catch {save e4} result; puts $result
quit -noprompt
