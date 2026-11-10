# CELL-TS-08: A read-only child with a DRC violation (its DRC result pending a stamp) under a parent; check, catchup; modify the parent; writeall force.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load input/parent} result; puts $result
catch {select top cell} result; puts $result
catch {expand} result; puts $result
catch {cellname writeable child false} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {cellname flags child} result; puts $result
catch {cellname list modified} result; puts $result
catch {box 20 0 24 4} result; puts $result
catch {paint metal1} result; puts $result
catch {writeall force} result; puts $result
catch {cellname flags child} result; puts $result
catch {cellname flags parent} result; puts $result
quit -noprompt
