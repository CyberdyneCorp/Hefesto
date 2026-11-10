# DRC-PARSE-2: width m1 -2: tech load lists drc among the sections with errors and fails.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
quit -noprompt
