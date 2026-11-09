# MODEL-G-023: split nw 0 0 5 5, split nw 5 5 10 10, metal1 0 5 5 10.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 5 5} result; puts $result
catch {splitpaint nw metal1} result; puts $result
catch {box 5 5 10 10} result; puts $result
catch {splitpaint nw metal1} result; puts $result
catch {box 0 5 5 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save a} result; puts $result
quit -noprompt
