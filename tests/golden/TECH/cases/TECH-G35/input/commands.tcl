# TECH-G35: scalegrid 1 2, then tech load of a tech with a drc width rule of 3 (tech drc query not authored).
# Reference script (Magic, Tcl): one result line per command.
catch {scalegrid 1 2} result; puts $result
catch {tech load input/tiny -noprompt} result; puts $result
catch {tech lambda} result; puts $result
quit -noprompt
