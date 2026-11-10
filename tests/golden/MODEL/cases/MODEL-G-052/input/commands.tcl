# MODEL-G-052: metal1, via1 and metal2 in a run; select net from the metal1.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 12 4} result; puts $result
catch {paint metal1} result; puts $result
catch {box 8 0 12 4} result; puts $result
catch {paint via1} result; puts $result
catch {box 8 0 30 4} result; puts $result
catch {paint metal2} result; puts $result
catch {box 1 1 1 1} result; puts $result
catch {select net} result; puts $result
catch {what -list} result; puts $result
catch {select bbox} result; puts $result
quit -noprompt
