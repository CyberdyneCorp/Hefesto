# DRC-SP-SUR-2: spacing poly ndiff 2 surround_ok (same plane): tech-load warning, printrules.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {drc printrules rules.txt} result; puts $result
quit -noprompt
