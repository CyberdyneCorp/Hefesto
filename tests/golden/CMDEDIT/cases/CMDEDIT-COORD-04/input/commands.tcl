# CMDEDIT-COORD-04: tech fine; units internal; box width with suffixed, fractional, negative and exponent literals.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/fine -noprompt} result; puts $result
catch {load a} result; puts $result
catch {units internal} result; puts $result
catch {box values 0 0 10 10} result; puts $result
catch {box width 1.5um} result; puts $result
catch {box width} result; puts $result
catch {box width 0.005um} result; puts $result
catch {box width} result; puts $result
catch {box width 25nm} result; puts $result
catch {box width} result; puts $result
catch {box width -0.005um} result; puts $result
catch {box width} result; puts $result
catch {box width 2.5} result; puts $result
catch {box width} result; puts $result
catch {box width -2.5} result; puts $result
catch {box width} result; puts $result
catch {box width 1e2} result; puts $result
catch {box width} result; puts $result
quit -noprompt
