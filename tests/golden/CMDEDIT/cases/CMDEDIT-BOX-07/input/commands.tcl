# CMDEDIT-BOX-07: Edit a child placed rotated 90; root box 0 0 10 20; -edit queries and setter.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load child} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save child} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell child 90} result; puts $result
catch {select cell child_0} result; puts $result
catch {edit} result; puts $result
catch {box values 0 0 10 20} result; puts $result
catch {box width -edit} result; puts $result
catch {box values -edit} result; puts $result
catch {box values 0 0 5 5 -edit} result; puts $result
catch {box values} result; puts $result
quit -noprompt
