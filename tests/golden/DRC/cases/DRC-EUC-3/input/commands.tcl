# DRC-EUC-3: spacing m1c m1c 4 touching_illegal; m1c (VIA1 squares border 5) diagonal neighbors; euclidean on.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {paint m1c} result; puts $result
catch {box 22 23 42 43} result; puts $result
catch {paint m1c} result; puts $result
catch {box 60 0 80 20} result; puts $result
catch {paint m1c} result; puts $result
catch {box 83 23 103 43} result; puts $result
catch {paint m1c} result; puts $result
catch {box 120 0 140 20} result; puts $result
catch {paint m1c} result; puts $result
catch {box 141 21 161 41} result; puts $result
catch {paint m1c} result; puts $result
catch {box 200 0 220 20} result; puts $result
catch {paint m1c} result; puts $result
catch {box 222 0 242 20} result; puts $result
catch {paint m1c} result; puts $result
catch {drc euclidean on} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
