# CMDEDIT-PAINT-09: Stacked contact pm12c: paint it; erase it; erase the user contact pc (Q-CMDEDIT-16).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load s1} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint pm12c} result; puts $result
catch {save s1} result; puts $result
catch {load s2} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint pm12c} result; puts $result
catch {erase pm12c} result; puts $result
catch {save s2} result; puts $result
catch {load s3} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint pm12c} result; puts $result
catch {erase pc} result; puts $result
catch {save s3} result; puts $result
quit -noprompt
