# UI-G-013: macro copy mytool; tool mytool; macro list.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {macro copy mytool} result; puts $result
catch {tool mytool} result; puts $result
catch {macro list} result; puts $result
quit -noprompt
