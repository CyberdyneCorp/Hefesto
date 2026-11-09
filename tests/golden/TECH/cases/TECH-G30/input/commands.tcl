# TECH-G30: styletype naming a family that does not exist, in a -dnull session (Q-TECH-24).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {tech name} result; puts $result
quit -noprompt
