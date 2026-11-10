# MODEL-G-040: Paint, save, undo; modified flag and the re-saved cell (Q-MODEL-12).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save a} result; puts $result
catch {undo} result; puts $result
catch {cellname list modified} result; puts $result
catch {save a2} result; puts $result
quit -noprompt
