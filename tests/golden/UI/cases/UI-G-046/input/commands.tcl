# UI-G-046: tool x4; tool foo; tool w; tool info; tool type (Tcl form).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {tool} result; puts $result
catch {tool} result; puts $result
catch {tool} result; puts $result
catch {tool} result; puts $result
catch {tool foo} result; puts $result
catch {tool w} result; puts $result
catch {tool info} result; puts $result
catch {tool type} result; puts $result
catch {tool} result; puts $result
quit -noprompt
