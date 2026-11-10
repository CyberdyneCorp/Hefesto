# CMDEDIT-COORD-02: tech fine; snap lambda; box values 0 0 100+5 1 (Q-CMDEDIT-1).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/fine -noprompt} result; puts $result
catch {load a} result; puts $result
catch {snap lambda} result; puts $result
catch {box values 0 0 100+5 1} result; puts $result
catch {box values} result; puts $result
quit -noprompt
