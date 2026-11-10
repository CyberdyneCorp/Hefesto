# DRC-CMD-1: drc c, drc halo, drc *halo, drc why extra, drc help, drc help wizard, drc, drc list, drc STATUS.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {drc c} result; puts $result
catch {drc halo} result; puts $result
catch {drc *halo} result; puts $result
catch {drc why extra} result; puts $result
catch {drc help} result; puts $result
catch {drc help wizard} result; puts $result
catch {drc} result; puts $result
catch {drc list} result; puts $result
catch {drc listall} result; puts $result
catch {drc STATUS} result; puts $result
quit -noprompt
