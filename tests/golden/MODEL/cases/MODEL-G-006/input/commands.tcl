# MODEL-G-006: drctest; spacing m1 m1 3: two spacing violations whose error regions are L-shaped; drc count, drc listall why and the error tiles of the saved cell.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 12 0 20 12} result; puts $result
catch {paint m1} result; puts $result
catch {box 0 12 20 20} result; puts $result
catch {paint m1} result; puts $result
catch {box 40 0 50 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 52 0 60 12} result; puts $result
catch {paint m1} result; puts $result
catch {box 40 12 60 20} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc count} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save a} result; puts $result
quit -noprompt
