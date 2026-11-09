# MODEL-G-028: Child lib/nand2 loaded through its path and placed twice (Q-MODEL-8).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell input/lib/nand2} result; puts $result
catch {box 20 0 20 0} result; puts $result
catch {getcell input/lib/nand2} result; puts $result
catch {save top} result; puts $result
quit -noprompt
