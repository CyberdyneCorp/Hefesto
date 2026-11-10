# DRC-CIF-1: cifwidth MET1 140 without cifstyle: load messages.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {drc printrules rules.txt} result; puts $result
quit -noprompt
