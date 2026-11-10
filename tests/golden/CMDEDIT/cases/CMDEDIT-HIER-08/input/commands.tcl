# CMDEDIT-HIER-08: cellname list children/parents/allcells/topcells, exists, instance orientation -def, rename, writeable.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save a} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell a} result; puts $result
catch {box 20 0 30 10} result; puts $result
catch {paint metal2} result; puts $result
catch {cellname list children top} result; puts $result
catch {cellname list parents a} result; puts $result
catch {cellname list allcells} result; puts $result
catch {cellname list topcells} result; puts $result
catch {cellname exists nosuch} result; puts $result
catch {instance orientation a_0 -def} result; puts $result
catch {cellname rename a b} result; puts $result
catch {cellname list allcells} result; puts $result
catch {cellname writeable b no} result; puts $result
catch {cellname writeable b} result; puts $result
quit -noprompt
