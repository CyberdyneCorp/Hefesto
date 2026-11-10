# DRC-EXT-3: extend nfet poly 3 exclusive; gates 2 long followed by poly, and a full gate.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint ndiff} result; puts $result
catch {box 4 -3 6 5} result; puts $result
catch {paint poly} result; puts $result
catch {box 8 -3 10 5} result; puts $result
catch {paint poly} result; puts $result
catch {box 30 0 40 10} result; puts $result
catch {paint ndiff} result; puts $result
catch {box 34 -3 36 13} result; puts $result
catch {paint poly} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
