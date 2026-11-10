# UI-G-012: tool wiring; macro Button1; tool box; macro Button1.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {tool wiring} result; puts $result
catch {macro Button1} result; puts $result
catch {tool box} result; puts $result
catch {macro Button1} result; puts $result
quit -noprompt
