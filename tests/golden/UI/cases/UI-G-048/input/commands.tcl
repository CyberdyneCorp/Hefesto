# UI-G-048: cursor, cursor lambda, cursor window, cursor 22, cursor bogus.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {setpoint 100 100 0} result; puts $result
catch {cursor} result; puts $result
catch {cursor lambda} result; puts $result
catch {cursor window} result; puts $result
catch {cursor 22} result; puts $result
catch {cursor bogus} result; puts $result
quit -noprompt
