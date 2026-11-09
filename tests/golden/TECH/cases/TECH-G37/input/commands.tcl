# TECH-G37: cifoutput scalefactor 5, gridlimit 5; scalegrid 1 2 and 1 20.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {scalegrid 1 2} result; puts $result
catch {scalegrid 1 20} result; puts $result
quit -noprompt
