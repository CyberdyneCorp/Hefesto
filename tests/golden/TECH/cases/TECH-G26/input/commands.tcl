# TECH-G26: poly connected to pc, pc to metal1, poly touching metal1: select net from the poly, then from metal1.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 4} result; puts $result
catch {paint poly} result; puts $result
catch {box 10 0 14 4} result; puts $result
catch {paint pc} result; puts $result
catch {box 10 0 30 4} result; puts $result
catch {paint metal1} result; puts $result
catch {box 30 0 40 4} result; puts $result
catch {paint poly} result; puts $result
catch {box 1 1 1 1} result; puts $result
catch {select net} result; puts $result
catch {what -list} result; puts $result
catch {select bbox} result; puts $result
catch {select clear} result; puts $result
catch {box 25 2 25 2} result; puts $result
catch {select net} result; puts $result
catch {what -list} result; puts $result
catch {select bbox} result; puts $result
quit -noprompt
