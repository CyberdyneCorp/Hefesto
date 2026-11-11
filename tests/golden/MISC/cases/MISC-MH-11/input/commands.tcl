# MISC-MH-11: flatten -doproperty of a top cell with the flatten property: a kept instance (keep, hint K) and a flattened one (fl, hint F, with a flattened leaf, hint X): Magic copies only K, Hefesto F and X as well (Q-MISC-15).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/t -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load leaf} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X 0 0 2 2} result; puts $result
catch {property flatten true} result; puts $result
catch {save leaf} result; puts $result
catch {load keep} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_K 0 0 2 2} result; puts $result
catch {save keep} result; puts $result
catch {load fl} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_F 0 0 3 3} result; puts $result
catch {property flatten true} result; puts $result
catch {box 6 0 6 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {save fl} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint m1} result; puts $result
catch {box 20 0 20 0} result; puts $result
catch {getcell keep} result; puts $result
catch {box 40 0 40 0} result; puts $result
catch {getcell fl} result; puts $result
catch {property flatten true} result; puts $result
catch {save top} result; puts $result
catch {load top} result; puts $result
catch {flatten -doproperty flat} result; puts $result
catch {load flat} result; puts $result
catch {save flat} result; puts $result
catch {property MASKHINTS_K} result; puts $result
catch {property MASKHINTS_F} result; puts $result
catch {property MASKHINTS_X} result; puts $result
quit -noprompt
