# MODEL-G-022: split nw then ne on 0 0 10 10 (a) and on a 1x5 box (b) (Q-MODEL-4).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {splitpaint nw metal1} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {splitpaint ne metal1} result; puts $result
catch {save a} result; puts $result
catch {load b} result; puts $result
catch {box 0 0 1 5} result; puts $result
catch {splitpaint nw metal1} result; puts $result
catch {box 0 0 1 5} result; puts $result
catch {splitpaint ne metal1} result; puts $result
catch {save b} result; puts $result
quit -noprompt
