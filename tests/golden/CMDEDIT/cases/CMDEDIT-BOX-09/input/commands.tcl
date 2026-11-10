# CMDEDIT-BOX-09: Two box values, undo, box values.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box values 0 0 10 10} result; puts $result
catch {box values 5 5 20 20} result; puts $result
catch {undo} result; puts $result
catch {box values} result; puts $result
quit -noprompt
