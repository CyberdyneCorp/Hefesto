# DRC-EUCL-CMD-1: drc euclidean; on; query; yes; query; off; query.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {drc euclidean} result; puts $result
catch {drc euclidean on} result; puts $result
catch {drc euclidean} result; puts $result
catch {drc euclidean yes} result; puts $result
catch {drc euclidean} result; puts $result
catch {drc euclidean on} result; puts $result
catch {drc euclidean off} result; puts $result
catch {drc euclidean} result; puts $result
quit -noprompt
