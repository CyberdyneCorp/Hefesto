# UI-G-011: macro ^R, macro Control_r, macro Shift_a "box", macro A.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {macro ^R} result; puts $result
catch {macro Control_r} result; puts $result
catch {macro Shift_a "box"} result; puts $result
catch {macro A} result; puts $result
quit -noprompt
