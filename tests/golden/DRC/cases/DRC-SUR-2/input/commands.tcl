# DRC-SUR-2: surround via m2 1 absence_illegal; via without m2; via surrounded by 1.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint via} result; puts $result
catch {box 9 -1 15 5} result; puts $result
catch {paint m2} result; puts $result
catch {box 10 0 14 4} result; puts $result
catch {paint via} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
