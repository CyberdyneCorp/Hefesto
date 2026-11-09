# MODEL-G-021: metal1 stripe 0 1 10 2, then splitpaint nw (cell a) and ne (cell b) over 0 0 10 3 (Q-MODEL-3).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 1 10 2} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 3} result; puts $result
catch {splitpaint nw metal1} result; puts $result
catch {save a} result; puts $result
catch {load b} result; puts $result
catch {box 0 1 10 2} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 3} result; puts $result
catch {splitpaint ne metal1} result; puts $result
catch {save b} result; puts $result
quit -noprompt
