# UI-G-004: ZoOm 2, then view get.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {view get} result; puts $result
catch {ZoOm 2} result; puts $result
catch {view get} result; puts $result
quit -noprompt
