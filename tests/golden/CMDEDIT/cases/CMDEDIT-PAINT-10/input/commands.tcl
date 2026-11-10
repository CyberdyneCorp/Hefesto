# CMDEDIT-PAINT-10: corner n e metal1 bevel in the root cell, and in a child edit cell placed at 100 0 (Q-CMDEDIT-15).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load r} result; puts $result
catch {box 5 -10 8 0} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {corner n e metal1 bevel} result; puts $result
catch {save r} result; puts $result
catch {load child} result; puts $result
catch {box 5 -10 8 0} result; puts $result
catch {paint metal1} result; puts $result
catch {save child} result; puts $result
catch {load top} result; puts $result
catch {box 100 0 100 0} result; puts $result
catch {getcell child} result; puts $result
catch {select cell child_0} result; puts $result
catch {edit} result; puts $result
catch {box 100 0 120 20} result; puts $result
catch {corner n e metal1 bevel} result; puts $result
catch {writeall force} result; puts $result
quit -noprompt
