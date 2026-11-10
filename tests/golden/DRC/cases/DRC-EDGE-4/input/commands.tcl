# DRC-EDGE-4: edge4way m1 space 4 ~m1/metal1 ~m1/metal1 2 (cdist < dist); diagonal neighbors; euclidean on.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 12 11 20 20} result; puts $result
catch {paint m1} result; puts $result
catch {box 30 0 40 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 43 13 50 20} result; puts $result
catch {paint m1} result; puts $result
catch {drc euclidean on} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
