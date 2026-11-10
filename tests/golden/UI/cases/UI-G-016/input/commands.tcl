# UI-G-016: logcommands log.txt; box; label "a b" n; setpoint 1 1; logcommands stop.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {logcommands log.txt} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {label "a b" n} result; puts $result
catch {setpoint 1 1} result; puts $result
catch {logcommands stop} result; puts $result
quit -noprompt
