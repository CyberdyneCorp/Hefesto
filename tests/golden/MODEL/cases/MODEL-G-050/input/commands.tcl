# MODEL-G-050: property x 1; undo; property x (property changes are not recorded with magic-compat on).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {property x 1} result; puts $result
catch {undo} result; puts $result
catch {property x} result; puts $result
quit -noprompt
