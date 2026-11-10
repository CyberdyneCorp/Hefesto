# DRC-SCALE-2: DRC-SCALE-1 rules, then scalegrid 1 2; printrules, *halo and *stepsize before and after.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {drc printrules rules_1.txt} result; puts $result
catch {drc *halo} result; puts $result
catch {drc *stepsize} result; puts $result
catch {scalegrid 1 2} result; puts $result
catch {drc printrules rules_2.txt} result; puts $result
catch {drc *halo} result; puts $result
catch {drc *stepsize} result; puts $result
quit -noprompt
