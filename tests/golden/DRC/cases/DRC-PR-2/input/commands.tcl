# DRC-PR-2: scmos: drc printrules of every DRC style (compared by digest).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {drc listall style} result; puts $result
catch {drc style default} result; puts $result
catch {drc printrules rules_0.txt} result; puts $result
catch {drc list style} result; puts $result
quit -noprompt
