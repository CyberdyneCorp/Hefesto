# TECH-G36: box 0 0 3 5 and a tile edge at x=-3; scalegrid 1 2; scalegrid 2 1; scalegrid 2/4, 1:3, 0 2.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box -3 0 3 5} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 3 5} result; puts $result
catch {scalegrid 1 2} result; puts $result
catch {box} result; puts $result
catch {save a1} result; puts $result
catch {scalegrid 2 1} result; puts $result
catch {box} result; puts $result
catch {save a2} result; puts $result
catch {scalegrid 2/4} result; puts $result
catch {box} result; puts $result
catch {scalegrid 1:3} result; puts $result
catch {box} result; puts $result
catch {scalegrid 0 2} result; puts $result
catch {box} result; puts $result
quit -noprompt
