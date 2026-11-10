# DRC-CIF-8: scalefactor 10 drc section with only mask-layer rules (Q-DRC-2): drc *halo and *stepsize.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {drc *halo} result; puts $result
catch {drc *stepsize} result; puts $result
quit -noprompt
