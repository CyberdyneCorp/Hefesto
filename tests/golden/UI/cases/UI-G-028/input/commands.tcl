# UI-G-028: center 50 50; center h 0.25; center 1.5 2.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {center 50 50} result; puts $result
catch {view get} result; puts $result
catch {center h 0.25} result; puts $result
catch {view get} result; puts $result
catch {center 1.5 2} result; puts $result
catch {view get} result; puts $result
quit -noprompt
