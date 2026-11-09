# MISC-FL-03: Child font label (rotation 30, offset 1 0) placed 90 and v; flatten; flatten -dotoplabels; flatten -doinplace -dotoplabels child_0; flatten -doinplace -nolabels -dotoplabels child_1 (Q-MISC-21).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load child} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label F FreeSans 2 30 1 0 center metal1} result; puts $result
catch {save child} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell child 90} result; puts $result
catch {box 40 0 40 0} result; puts $result
catch {getcell child v} result; puts $result
catch {save top} result; puts $result
catch {load top} result; puts $result
catch {flatten flat} result; puts $result
catch {load flat} result; puts $result
catch {save flat} result; puts $result
catch {load top} result; puts $result
catch {flatten -dotoplabels flat_t} result; puts $result
catch {load flat_t} result; puts $result
catch {save flat_t} result; puts $result
catch {load top} result; puts $result
catch {flatten -doinplace -dotoplabels child_0} result; puts $result
catch {flatten -doinplace -nolabels -dotoplabels child_1} result; puts $result
catch {save top} result; puts $result
quit -noprompt
