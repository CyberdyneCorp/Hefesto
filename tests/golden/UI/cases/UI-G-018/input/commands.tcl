# UI-G-018: setpoint 100 100 0, then setpoint with no arguments.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {setpoint 100 100 0} result; puts $result
catch {setpoint} result; puts $result
quit -noprompt
