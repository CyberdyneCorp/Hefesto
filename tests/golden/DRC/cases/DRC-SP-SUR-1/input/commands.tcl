# DRC-SP-SUR-1: spacing nwell pdiff 5 surround_ok; pdiff crossing the nwell edge, pdiff 3 outside, pdiff inside.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 30 30} result; puts $result
catch {paint nwell} result; puts $result
catch {box 25 10 35 15} result; puts $result
catch {paint pdiff} result; puts $result
catch {box 33 20 38 25} result; puts $result
catch {paint pdiff} result; puts $result
catch {box 10 10 15 15} result; puts $result
catch {paint pdiff} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
