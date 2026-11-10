# CMDEDIT-BOX-08: box position and values past the coordinate limit (Q-CMDEDIT-12).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {box position 2000000000i 0} result; puts $result
catch {box values} result; puts $result
catch {box values 0 0 2000000000i 1} result; puts $result
catch {box values} result; puts $result
quit -noprompt
