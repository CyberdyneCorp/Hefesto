# CMDEDIT-LBL-06: findlabel OUT; findlabel -glob V*; findlabel x1/A 1.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load x} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label A} result; puts $result
catch {save x} result; puts $result
catch {load a} result; puts $result
catch {box 50 0 50 0} result; puts $result
catch {getcell x} result; puts $result
catch {select cell x_0} result; puts $result
catch {identify x1} result; puts $result
catch {select clear} result; puts $result
catch {box 0 0 30 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 2 2 2 2} result; puts $result
catch {label OUT} result; puts $result
catch {box 12 2 12 2} result; puts $result
catch {label VDD} result; puts $result
catch {box 22 2 22 2} result; puts $result
catch {label VSS} result; puts $result
catch {findlabel OUT} result; puts $result
catch {box values} result; puts $result
catch {findlabel -glob V*} result; puts $result
catch {box values} result; puts $result
catch {findlabel x1/A 1} result; puts $result
catch {box values} result; puts $result
quit -noprompt
