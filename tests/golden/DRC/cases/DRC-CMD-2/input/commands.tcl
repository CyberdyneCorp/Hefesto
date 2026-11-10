# DRC-CMD-2: drc status; drc off; drc status; drc on; drc status.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc status} result; puts $result
catch {drc off} result; puts $result
catch {drc status} result; puts $result
catch {drc on} result; puts $result
catch {drc status} result; puts $result
quit -noprompt
