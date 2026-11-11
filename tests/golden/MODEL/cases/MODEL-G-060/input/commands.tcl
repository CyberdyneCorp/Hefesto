# MODEL-G-060: top with a port and no flatten property, child c with the flatten property; flatten -nosubcircuits, -doproperty, -nosubcircuits -dotoplabels and -novendor (Q-MODEL-33: the first two are empty, the third has only top's label P).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load c} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint metal2} result; puts $result
catch {box 2 2 2 2} result; puts $result
catch {label Q} result; puts $result
catch {property flatten true} result; puts $result
catch {save c} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label P} result; puts $result
catch {port make} result; puts $result
catch {box 20 0 20 0} result; puts $result
catch {getcell c} result; puts $result
catch {save top} result; puts $result
catch {load top} result; puts $result
catch {flatten -nosubcircuits f1} result; puts $result
catch {load f1} result; puts $result
catch {save f1} result; puts $result
catch {load top} result; puts $result
catch {flatten -doproperty f2} result; puts $result
catch {load f2} result; puts $result
catch {save f2} result; puts $result
catch {load top} result; puts $result
catch {flatten -nosubcircuits -dotoplabels f3} result; puts $result
catch {load f3} result; puts $result
catch {save f3} result; puts $result
catch {load top} result; puts $result
catch {flatten -novendor f4} result; puts $result
catch {load f4} result; puts $result
catch {save f4} result; puts $result
quit -noprompt
