# CMDEDIT-HIER-05: edit on an instance at translate 100 0; paint metal1; edit x1/x2; down (Q-CMDEDIT-30).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load c2} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save c2} result; puts $result
catch {load c1} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell c2} result; puts $result
catch {select cell c2_0} result; puts $result
catch {identify x2} result; puts $result
catch {save c1} result; puts $result
catch {load top} result; puts $result
catch {box 100 0 100 0} result; puts $result
catch {getcell c1} result; puts $result
catch {select cell c1_0} result; puts $result
catch {identify x1} result; puts $result
catch {select cell x1} result; puts $result
catch {edit} result; puts $result
catch {box 100 0 110 10} result; puts $result
catch {paint metal2} result; puts $result
catch {windowcaption} result; puts $result
catch {select clear} result; puts $result
catch {edit x1/x2} result; puts $result
catch {windowcaption} result; puts $result
catch {select cell x1} result; puts $result
catch {down} result; puts $result
catch {windowcaption} result; puts $result
catch {writeall force} result; puts $result
quit -noprompt
