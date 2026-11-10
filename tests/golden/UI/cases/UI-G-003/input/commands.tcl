# UI-G-003: foo in a layout window.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {box values} result; puts $result
catch {foo} result; puts $result
quit -noprompt
