# TECH-G38: cifoutput with options set-minimum-grid; tech lambda.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {tech lambda} result; puts $result
quit -noprompt
