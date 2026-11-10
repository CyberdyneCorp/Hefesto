# CMDEDIT-LBL-01: Cell bbox 0 0 90 90; label IN with the box in each of 9 regions.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 90 90} result; puts $result
catch {paint metal1} result; puts $result
catch {box 15 15 15 15} result; puts $result
catch {label IN} result; puts $result
catch {box 45 15 45 15} result; puts $result
catch {label IN} result; puts $result
catch {box 75 15 75 15} result; puts $result
catch {label IN} result; puts $result
catch {box 15 45 15 45} result; puts $result
catch {label IN} result; puts $result
catch {box 45 45 45 45} result; puts $result
catch {label IN} result; puts $result
catch {box 75 45 75 45} result; puts $result
catch {label IN} result; puts $result
catch {box 15 75 15 75} result; puts $result
catch {label IN} result; puts $result
catch {box 45 75 45 75} result; puts $result
catch {label IN} result; puts $result
catch {box 75 75 75 75} result; puts $result
catch {label IN} result; puts $result
catch {save a} result; puts $result
quit -noprompt
