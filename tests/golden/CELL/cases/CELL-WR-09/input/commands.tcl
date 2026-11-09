# CELL-WR-09: Properties of every type saved with property compat true and false.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {property s hello} result; puts $result
catch {property integer i 7} result; puts $result
catch {property dimension d 5 10} result; puts $result
catch {property double f 1.5} result; puts $result
catch {property FIXED_BBOX 0 0 10 20} result; puts $result
catch {property MASKHINTS_A 0 0 4 4} result; puts $result
catch {property compat true} result; puts $result
catch {save a1} result; puts $result
catch {property compat false} result; puts $result
catch {save a2} result; puts $result
quit -noprompt
