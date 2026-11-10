# DRC-EUC-4: spacing m1 m1 3 touching_illegal; a split m1 tile near the corner extension of m1; euclidean on.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 11 11 19 19} result; puts $result
catch {splitpaint sw m1} result; puts $result
catch {box 30 0 40 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 41 11 49 19} result; puts $result
catch {splitpaint ne m1} result; puts $result
catch {drc euclidean on} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
