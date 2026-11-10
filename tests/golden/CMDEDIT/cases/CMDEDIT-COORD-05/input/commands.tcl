# CMDEDIT-COORD-05: box width 5km; box width abc.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {box width 5km} result; puts $result
catch {box width} result; puts $result
catch {box width abc} result; puts $result
catch {box width} result; puts $result
quit -noprompt
