# DRC-HALO-1: widespacing m1 10 m1 3, no stepsize: *halo, *stepsize.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {drc *halo} result; puts $result
catch {drc *stepsize} result; puts $result
quit -noprompt
