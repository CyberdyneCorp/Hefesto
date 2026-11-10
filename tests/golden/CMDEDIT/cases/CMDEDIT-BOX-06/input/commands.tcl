# CMDEDIT-BOX-06: box exists; box remove; box exists; box width.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box values 0 0 10 10} result; puts $result
catch {box exists} result; puts $result
catch {box remove} result; puts $result
catch {box exists} result; puts $result
catch {box width} result; puts $result
quit -noprompt
