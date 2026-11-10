# CELL-TS-05: cellname timestamp a 5 after the last edit, save (the file keeps stamp 5); cellname timestamp b 5, then modify b and save (a new stamp, masked).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint metal1} result; puts $result
catch {cellname timestamp a 5} result; puts $result
catch {save a} result; puts $result
catch {cellname timestamp a} result; puts $result
catch {cellname flags a} result; puts $result
catch {load b} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint metal1} result; puts $result
catch {cellname timestamp b 5} result; puts $result
catch {box 10 0 14 4} result; puts $result
catch {paint metal1} result; puts $result
catch {save b} result; puts $result
catch {cellname flags b} result; puts $result
quit -noprompt
