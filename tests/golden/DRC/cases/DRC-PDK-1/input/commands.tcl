# DRC-PDK-1: sky130A: sky130_fd_sc_hd__inv_1 as installed, then the same cell with a deliberate met1 and li1 width violation; CAP of each.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/sky130A -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {scalegrid 1 2} result; puts $result
catch {drc off} result; puts $result
catch {drc euclidean on} result; puts $result
catch {snap internal} result; puts $result
catch {load input/sky130_fd_sc_hd__inv_1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {drc count} result; puts $result
catch {box 0 0 10 300} result; puts $result
catch {paint met1} result; puts $result
catch {box 1000 0 1010 50} result; puts $result
catch {paint li1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save inv_broken} result; puts $result
quit -noprompt
