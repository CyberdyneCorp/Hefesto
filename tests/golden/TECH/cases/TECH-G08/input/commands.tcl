# TECH-G08: include of a sibling file from the tech directory; include of a file with no .tech suffix.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny_inc -noprompt} result; puts $result
catch {tech name} result; puts $result
catch {tech load input/tiny2 -noprompt} result; puts $result
catch {tech name} result; puts $result
quit -noprompt
