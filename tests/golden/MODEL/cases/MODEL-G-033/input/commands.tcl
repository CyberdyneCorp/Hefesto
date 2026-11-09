# MODEL-G-033: flatten -nosubcircuits with a ported and an unported child.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load pc} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label A} result; puts $result
catch {port make} result; puts $result
catch {save pc} result; puts $result
catch {load uc} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal2} result; puts $result
catch {save uc} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell pc} result; puts $result
catch {box 20 0 20 0} result; puts $result
catch {getcell uc} result; puts $result
catch {save top} result; puts $result
catch {load top} result; puts $result
catch {flatten -nosubcircuits flat} result; puts $result
catch {load flat} result; puts $result
catch {save flat} result; puts $result
quit -noprompt
