# DRC-EUC-5: spacing m1 m1 5000 touching_illegal; diagonal neighbors at distance 5000 and just under (Q-DRC-20).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 3010 4010 3020 4020} result; puts $result
catch {paint m1} result; puts $result
catch {box 20000 0 20010 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 23546 3545 23556 3555} result; puts $result
catch {paint m1} result; puts $result
catch {drc euclidean on} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
