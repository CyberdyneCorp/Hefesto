# DRC-RECT-2: rectangle via 3 any; via 5x2; non-rectangular via.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 5 2} result; puts $result
catch {paint via} result; puts $result
catch {box 10 0 14 2} result; puts $result
catch {paint via} result; puts $result
catch {box 10 0 12 4} result; puts $result
catch {paint via} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
