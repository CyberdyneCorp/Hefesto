# MODEL-G-024: Child via1 0 0 4 4 placed at 100 0 over parent metal2 100 0 120 4; flatten.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load child} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint via1} result; puts $result
catch {save child} result; puts $result
catch {load top} result; puts $result
catch {box 100 0 120 4} result; puts $result
catch {paint metal2} result; puts $result
catch {box 100 0 100 0} result; puts $result
catch {getcell child} result; puts $result
catch {save top} result; puts $result
catch {load top} result; puts $result
catch {flatten flat} result; puts $result
catch {load flat} result; puts $result
catch {save flat} result; puts $result
quit -noprompt
