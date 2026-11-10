# DRC-SUR-5: surround via m1 2 5 directional; surround 1 left/right and 3 top/bottom; and 1 everywhere.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box -1 -3 5 7} result; puts $result
catch {paint m1} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint via} result; puts $result
catch {box 19 -1 25 5} result; puts $result
catch {paint m1} result; puts $result
catch {box 20 0 24 4} result; puts $result
catch {paint via} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
