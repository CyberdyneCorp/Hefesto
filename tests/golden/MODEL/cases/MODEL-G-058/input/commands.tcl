# MODEL-G-058: Three paints in one box, undo twice, paint, redo (nothing to redo).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {paint metal2} result; puts $result
catch {paint metal3} result; puts $result
catch {undo} result; puts $result
catch {undo} result; puts $result
catch {paint ndiff} result; puts $result
catch {redo} result; puts $result
catch {save a} result; puts $result
quit -noprompt
