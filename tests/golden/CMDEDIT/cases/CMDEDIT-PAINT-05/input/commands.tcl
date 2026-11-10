# CMDEDIT-PAINT-05: Three instances, two overlapping the box; erase subcell.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load leaf} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save leaf} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {box 20 0 20 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {box 40 0 40 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {box 5 5 25 15} result; puts $result
catch {erase subcell} result; puts $result
catch {save top} result; puts $result
quit -noprompt
