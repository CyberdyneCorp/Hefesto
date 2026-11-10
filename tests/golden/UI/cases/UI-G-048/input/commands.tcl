# UI-G-048: cursor 22, cursor bogus (the cursor queries read the X pointer, uninitialised on Magic's null display: dropped per Q-UI-43).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {setpoint 100 100 0} result; puts $result
catch {cursor 22} result; puts $result
catch {cursor bogus} result; puts $result
quit -noprompt
