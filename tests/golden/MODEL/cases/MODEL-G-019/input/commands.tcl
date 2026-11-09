# MODEL-G-019: ndiff 0 0 10 10, poly 4 -2 6 12, then erase poly 4 0 6 10.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint ndiff} result; puts $result
catch {box 4 -2 6 12} result; puts $result
catch {paint poly} result; puts $result
catch {save a1} result; puts $result
catch {box 4 0 6 10} result; puts $result
catch {erase poly} result; puts $result
catch {save a2} result; puts $result
quit -noprompt
