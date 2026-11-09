# MODEL-G-042: A .mag with a label on via1+via2; load and save.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/s} result; puts $result
catch {save s} result; puts $result
quit -noprompt
