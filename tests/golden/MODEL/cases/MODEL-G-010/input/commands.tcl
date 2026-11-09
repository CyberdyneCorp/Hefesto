# MODEL-G-010: Child labels with the 8 justifications, placed 90 and v, flattened.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load child} result; puts $result
catch {box 0 0 30 30} result; puts $result
catch {paint metal1} result; puts $result
catch {box 3 3 3 3} result; puts $result
catch {label jn n} result; puts $result
catch {box 6 6 6 6} result; puts $result
catch {label jne ne} result; puts $result
catch {box 9 9 9 9} result; puts $result
catch {label je e} result; puts $result
catch {box 12 12 12 12} result; puts $result
catch {label jse se} result; puts $result
catch {box 15 15 15 15} result; puts $result
catch {label js s} result; puts $result
catch {box 18 18 18 18} result; puts $result
catch {label jsw sw} result; puts $result
catch {box 21 21 21 21} result; puts $result
catch {label jw w} result; puts $result
catch {box 24 24 24 24} result; puts $result
catch {label jnw nw} result; puts $result
catch {save child} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell child 90} result; puts $result
catch {box 100 0 100 0} result; puts $result
catch {getcell child v} result; puts $result
catch {save top} result; puts $result
catch {load top} result; puts $result
catch {flatten flat} result; puts $result
catch {load flat} result; puts $result
catch {save flat} result; puts $result
quit -noprompt
