# CMDEDIT-SEL-04: L-shaped metal1; select chunk with the box ll at 5 5.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 100 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 50} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 6 6} result; puts $result
catch {select chunk} result; puts $result
catch {select bbox} result; puts $result
catch {box values} result; puts $result
quit -noprompt
