# MODEL-G-002: Two stacked squares (a); a square plus an offset square (b).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 10 10 20} result; puts $result
catch {paint metal1} result; puts $result
catch {save a} result; puts $result
catch {load b} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 10 15 20} result; puts $result
catch {paint metal1} result; puts $result
catch {save b} result; puts $result
quit -noprompt
