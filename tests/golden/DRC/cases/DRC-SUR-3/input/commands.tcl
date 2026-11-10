# DRC-SUR-3: surround pc poly 1 absence_illegal (same plane); pc without poly, with poly, with 0 on one side.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint pc} result; puts $result
catch {box 9 -1 15 5} result; puts $result
catch {paint poly} result; puts $result
catch {box 10 0 14 4} result; puts $result
catch {paint pc} result; puts $result
catch {box 20 -1 25 5} result; puts $result
catch {paint poly} result; puts $result
catch {box 20 0 24 4} result; puts $result
catch {paint pc} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
