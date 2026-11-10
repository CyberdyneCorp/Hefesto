# MISC-MH-09: A properties section with the type keyword plane, PLANE or str before string BAZ qux (Q-MISC-12).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/t -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load input/P} result; puts $result
catch {property} result; puts $result
catch {load input/Q} result; puts $result
catch {property} result; puts $result
catch {load input/R} result; puts $result
catch {property} result; puts $result
quit -noprompt
