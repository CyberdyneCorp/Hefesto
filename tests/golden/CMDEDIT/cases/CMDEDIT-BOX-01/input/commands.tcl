# CMDEDIT-BOX-01: box values 10 10 20 30; width 5, height -4, position 0 -5, size 7 8 (box values after each).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box values 10 10 20 30} result; puts $result
catch {box width 5} result; puts $result
catch {box values} result; puts $result
catch {box height -4} result; puts $result
catch {box values} result; puts $result
catch {box position 0 -5} result; puts $result
catch {box values} result; puts $result
catch {box size 7 8} result; puts $result
catch {box values} result; puts $result
quit -noprompt
