# DRC-SCALE-3: scalefactor 300 width m1 451, then scalegrid 1 2 and 2 1; and (Q-DRC-1) a second technology with scalefactor 10 area m1 1001 2, then scalegrid 1 2; printrules before and after each rescale.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {drc printrules w_1.txt} result; puts $result
catch {scalegrid 1 2} result; puts $result
catch {drc printrules w_2.txt} result; puts $result
catch {scalegrid 2 1} result; puts $result
catch {drc printrules w_3.txt} result; puts $result
catch {tech load input/drcarea -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {tech lambda} result; puts $result
catch {drc printrules a_1.txt} result; puts $result
catch {scalegrid 1 2} result; puts $result
catch {drc printrules a_2.txt} result; puts $result
catch {drc *halo} result; puts $result
quit -noprompt
