# CMDEDIT-COORD-08: snap internal, units, snap; units lambda; snap user; snap; units.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {snap internal} result; puts $result
catch {units} result; puts $result
catch {snap} result; puts $result
catch {units lambda} result; puts $result
catch {snap user} result; puts $result
catch {snap} result; puts $result
catch {units} result; puts $result
quit -noprompt
