# CMDEDIT-LBL-05: property FIXED_BBOX "0 0 100 200", property, foo bar, foo "", integer n 1 2, compat (Q-CMDEDIT-93).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {property FIXED_BBOX "0 0 100 200"} result; puts $result
catch {property} result; puts $result
catch {property foo bar} result; puts $result
catch {property foo} result; puts $result
catch {property foo ""} result; puts $result
catch {property} result; puts $result
catch {property integer n 1 2} result; puts $result
catch {property n} result; puts $result
catch {property compat} result; puts $result
catch {save a} result; puts $result
quit -noprompt
