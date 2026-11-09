# MODEL-G-029: A .mag with two uses sharing an id and one without an id; load and save.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/dup} result; puts $result
catch {save dup} result; puts $result
quit -noprompt
