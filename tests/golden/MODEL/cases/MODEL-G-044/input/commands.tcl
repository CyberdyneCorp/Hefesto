# MODEL-G-044: FreeSans flabels of sizes 8/64/128, every justification, rotation 45; parent use box (Q-MODEL-6).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell input/f} result; puts $result
catch {save top} result; puts $result
quit -noprompt
