# MODEL-G-056: Move a use and undo (saved after each); move the box and undo.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load leaf} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save leaf} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {select cell leaf_0} result; puts $result
catch {move 10 0} result; puts $result
catch {save t1} result; puts $result
catch {undo} result; puts $result
catch {save t2} result; puts $result
catch {box values 0 0 5 5} result; puts $result
catch {box move e 10} result; puts $result
catch {box values} result; puts $result
catch {undo} result; puts $result
catch {box values} result; puts $result
quit -noprompt
