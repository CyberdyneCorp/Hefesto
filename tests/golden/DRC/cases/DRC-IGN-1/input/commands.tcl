# DRC-IGN-1: drc ignore spacing where the last-defined why contains spacing (Q-DRC-17); drc ignore; drc ignore none.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 12 0 22 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 0 20 10 30} result; puts $result
catch {paint m2} result; puts $result
catch {box 12 20 22 30} result; puts $result
catch {paint m2} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
catch {select top cell} result; puts $result
catch {drc why} result; puts $result
catch {drc ignore spacing} result; puts $result
catch {drc ignore} result; puts $result
catch {drc why} result; puts $result
catch {drc listall why} result; puts $result
catch {drc ignore none} result; puts $result
catch {drc ignore} result; puts $result
catch {drc why} result; puts $result
catch {drc ignore NONE} result; puts $result
quit -noprompt
