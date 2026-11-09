# UI-G-029: box 0 0 10 10; findbox zoom; findbox.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {findbox zoom} result; puts $result
catch {view get} result; puts $result
catch {findbox} result; puts $result
catch {view get} result; puts $result
quit -noprompt
