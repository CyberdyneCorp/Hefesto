# CMDEDIT-BOX-05: box corner ne 20 30 (Q-CMDEDIT-9).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {box corner ne 20 30} result; puts $result
catch {box values} result; puts $result
quit -noprompt
