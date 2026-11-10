# CMDEDIT-PAINT-07: fill n; fill n metal1; fill s; fill e; fill w.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load f1} result; puts $result
catch {box 2 -5 4 0} result; puts $result
catch {paint poly} result; puts $result
catch {box 6 5 8 8} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 20} result; puts $result
catch {fill n} result; puts $result
catch {save f1} result; puts $result
catch {load f2} result; puts $result
catch {box 2 -5 4 0} result; puts $result
catch {paint poly} result; puts $result
catch {box 6 5 8 8} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 20} result; puts $result
catch {fill n metal1} result; puts $result
catch {save f2} result; puts $result
catch {load f3} result; puts $result
catch {box 2 20 4 25} result; puts $result
catch {paint poly} result; puts $result
catch {box 0 0 10 20} result; puts $result
catch {fill s} result; puts $result
catch {save f3} result; puts $result
catch {load f4} result; puts $result
catch {box -5 2 0 4} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 20} result; puts $result
catch {fill e} result; puts $result
catch {save f4} result; puts $result
catch {load f5} result; puts $result
catch {box 10 2 15 4} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 20} result; puts $result
catch {fill w} result; puts $result
catch {save f5} result; puts $result
quit -noprompt
