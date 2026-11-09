# MODEL-G-047: A .mag with string GDS_START, FIXED_BBOX, MASKHINTS_X; property queries.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/p} result; puts $result
catch {property GDS_START} result; puts $result
catch {property FIXED_BBOX} result; puts $result
catch {property MASKHINTS_X} result; puts $result
catch {property} result; puts $result
quit -noprompt
