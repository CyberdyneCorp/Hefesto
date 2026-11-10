# CMDEDIT-BOX-03: box 0 0 100 50 under tiny (lambda 1) and under tech fine.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 100 50} result; puts $result
catch {tech load input/fine -noprompt} result; puts $result
catch {load b} result; puts $result
catch {box 0 0 100 50} result; puts $result
quit -noprompt
