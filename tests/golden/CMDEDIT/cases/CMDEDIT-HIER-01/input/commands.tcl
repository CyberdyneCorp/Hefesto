# CMDEDIT-HIER-01: Edit cell holding inv_0 and inv_2; getcell inv.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/top} result; puts $result
catch {box 50 0 50 0} result; puts $result
catch {getcell inv} result; puts $result
catch {save top} result; puts $result
quit -noprompt
