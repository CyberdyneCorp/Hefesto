# CMDEDIT-MISC-04: box 0 0 200 100; findbox zoom; view.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 300 300} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 200 100} result; puts $result
catch {findbox zoom} result; puts $result
catch {view get} result; puts $result
quit -noprompt
