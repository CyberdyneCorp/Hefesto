# CMDEDIT-SEL-11: move origin e 10.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {move origin e 10} result; puts $result
catch {box values} result; puts $result
catch {save a} result; puts $result
quit -noprompt
