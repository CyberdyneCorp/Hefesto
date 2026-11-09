# UI-G-024: Load a cell with bbox 0 0 100 50; view get.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {view get} result; puts $result
quit -noprompt
