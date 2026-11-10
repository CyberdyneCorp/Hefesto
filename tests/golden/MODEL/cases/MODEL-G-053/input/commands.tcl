# MODEL-G-053: Parent metal1 abutting a child whose two VDD ports share index 1 and are not connected; select net.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load child} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 1 1 1 1} result; puts $result
catch {label VDD} result; puts $result
catch {port make 1} result; puts $result
catch {box 20 0 30 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 21 1 21 1} result; puts $result
catch {label VDD} result; puts $result
catch {port make 1} result; puts $result
catch {save child} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell child} result; puts $result
catch {box -10 0 0 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box -10 0 30 10} result; puts $result
catch {expand} result; puts $result
catch {box -5 5 -5 5} result; puts $result
catch {select net} result; puts $result
catch {what -list} result; puts $result
catch {select bbox} result; puts $result
quit -noprompt
