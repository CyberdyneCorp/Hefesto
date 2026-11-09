# MODEL-G-017: pdc 0 0 4 4, then ndiff over it.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint pdc} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint ndiff} result; puts $result
catch {save a} result; puts $result
quit -noprompt
