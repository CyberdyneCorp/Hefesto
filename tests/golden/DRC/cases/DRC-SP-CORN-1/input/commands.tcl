# DRC-SP-CORN-1: spacing poly ndiff 2 corner_ok nfet; poly meeting ndiff at an nfet corner; a 1-unit poly-ndiff gap.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint ndiff} result; puts $result
catch {box 4 -3 6 13} result; puts $result
catch {paint poly} result; puts $result
catch {box 6 10 12 13} result; puts $result
catch {paint poly} result; puts $result
catch {box 30 0 40 10} result; puts $result
catch {paint ndiff} result; puts $result
catch {box 41 0 45 10} result; puts $result
catch {paint poly} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
