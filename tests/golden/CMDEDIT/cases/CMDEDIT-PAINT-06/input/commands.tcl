# CMDEDIT-PAINT-06: Zero-area box on a point label, on a label border and inside a rect label; erase labels (Q-CMDEDIT-20).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 30 30} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label P} result; puts $result
catch {box 10 2 10 2} result; puts $result
catch {label E} result; puts $result
catch {box 10 10 20 20} result; puts $result
catch {label R} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {erase labels} result; puts $result
catch {save l1} result; puts $result
catch {box 10 15 10 15} result; puts $result
catch {erase labels} result; puts $result
catch {save l2} result; puts $result
catch {box 15 15 15 15} result; puts $result
catch {erase labels} result; puts $result
catch {save l3} result; puts $result
quit -noprompt
