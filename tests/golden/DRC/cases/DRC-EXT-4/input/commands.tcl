# DRC-EXT-4: extend m1 poly 3 (layers on different planes, trigger rules); printrules; CAP.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 4} result; puts $result
catch {paint poly} result; puts $result
catch {box 2 -1 8 5} result; puts $result
catch {paint m1} result; puts $result
catch {box 20 0 30 4} result; puts $result
catch {paint poly} result; puts $result
catch {box 22 -4 28 8} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
catch {drc printrules rules.txt} result; puts $result
quit -noprompt
