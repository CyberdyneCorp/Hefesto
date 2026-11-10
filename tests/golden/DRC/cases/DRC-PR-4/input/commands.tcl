# DRC-PR-4: gf180mcuD under `scalegrid 1 10`: drc printrules of every DRC style (compared by digest).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/gf180mcuD -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {scalegrid 1 10} result; puts $result
catch {drc off} result; puts $result
catch {drc euclidean on} result; puts $result
catch {snap internal} result; puts $result
catch {drc listall style} result; puts $result
catch {drc style drc(fast)} result; puts $result
catch {drc printrules rules_0.txt} result; puts $result
catch {drc style drc(full)} result; puts $result
catch {drc printrules rules_1.txt} result; puts $result
catch {drc style drc(routing)} result; puts $result
catch {drc printrules rules_2.txt} result; puts $result
catch {drc style empty} result; puts $result
catch {drc printrules rules_3.txt} result; puts $result
catch {drc list style} result; puts $result
quit -noprompt
