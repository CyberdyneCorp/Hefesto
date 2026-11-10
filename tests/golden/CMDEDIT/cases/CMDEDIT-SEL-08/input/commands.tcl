# CMDEDIT-SEL-08: Selection of edit metal1 and child poly; delete.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load c} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint poly} result; puts $result
catch {save c} result; puts $result
catch {load top} result; puts $result
catch {box 20 0 20 0} result; puts $result
catch {getcell c} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box -5 -5 40 15} result; puts $result
catch {select area} result; puts $result
catch {delete} result; puts $result
catch {writeall force} result; puts $result
quit -noprompt
