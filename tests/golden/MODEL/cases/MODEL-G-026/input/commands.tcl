# MODEL-G-026: Save, then paint metal1 where metal1 already is; cellname list modified (Q-MODEL-23).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save a} result; puts $result
catch {cellname list modified} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {cellname list modified} result; puts $result
quit -noprompt
