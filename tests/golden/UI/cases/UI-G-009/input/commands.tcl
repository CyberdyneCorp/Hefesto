# UI-G-009: macro list and macro help with the default system startup file (box tool).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {macro list} result; puts $result
catch {macro help} result; puts $result
quit -noprompt
