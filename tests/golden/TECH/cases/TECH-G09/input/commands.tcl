# TECH-G09: A missing include inside connect (Q-TECH-2).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
quit -noprompt
