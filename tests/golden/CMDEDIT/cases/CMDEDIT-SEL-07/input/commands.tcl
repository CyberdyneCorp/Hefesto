# CMDEDIT-SEL-07: select no area, select s area, select simple labels, select foo bar (Q-CMDEDIT-60).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label L} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {select no area} result; puts $result
catch {what -list} result; puts $result
catch {select s area} result; puts $result
catch {what -list} result; puts $result
catch {select simple labels} result; puts $result
catch {select foo bar} result; puts $result
catch {what -list} result; puts $result
quit -noprompt
