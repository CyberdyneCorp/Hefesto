# UI-G-019: setpoint 120 100 0; pushbutton left down; pushbutton left up; box (box tool).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {setpoint 120 100 0} result; puts $result
catch {pushbutton left down} result; puts $result
catch {pushbutton left up} result; puts $result
catch {box values} result; puts $result
quit -noprompt
