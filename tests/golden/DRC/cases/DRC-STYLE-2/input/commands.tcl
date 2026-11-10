# DRC-STYLE-2: Rules before any style line (implicit default), then style other; list styles, printrules both.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {drc list style} result; puts $result
catch {drc listall style} result; puts $result
catch {drc printrules rules_default.txt} result; puts $result
catch {drc style other} result; puts $result
catch {drc list style} result; puts $result
catch {drc printrules rules_other.txt} result; puts $result
quit -noprompt
