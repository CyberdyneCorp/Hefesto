# DRC-STAT-1: sky130A: drc rulestats; check sky130_fd_sc_hd__inv_1; drc statistics.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/sky130A -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {scalegrid 1 2} result; puts $result
catch {drc off} result; puts $result
catch {drc euclidean on} result; puts $result
catch {snap internal} result; puts $result
catch {drc rulestats} result; puts $result
catch {load input/sky130_fd_sc_hd__inv_1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {drc statistics} result; puts $result
quit -noprompt
