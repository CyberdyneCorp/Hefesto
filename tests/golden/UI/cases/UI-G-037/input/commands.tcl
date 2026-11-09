# UI-G-037: expand t, expand toggle, unexpand, xview on a cell with a child.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {load input/top} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {expand t} result; puts $result
catch {expand toggle} result; puts $result
catch {unexpand} result; puts $result
catch {xview} result; puts $result
catch {view get} result; puts $result
quit -noprompt
