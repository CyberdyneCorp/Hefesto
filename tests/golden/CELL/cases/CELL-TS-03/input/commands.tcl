# CELL-TS-03: Two parents with use timestamps 111 and 222 for the same unread child (file stamp 333).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load input/p1} result; puts $result
catch {cellname timestamp child} result; puts $result
catch {load input/p2} result; puts $result
catch {cellname timestamp child} result; puts $result
catch {select top cell} result; puts $result
catch {expand} result; puts $result
catch {cellname timestamp child} result; puts $result
quit -noprompt
