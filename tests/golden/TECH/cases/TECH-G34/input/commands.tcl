# TECH-G34: Cell with metal2 paint; reload a technology without metal2.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal2} result; puts $result
catch {box 0 0 5 5} result; puts $result
catch {paint metal1} result; puts $result
catch {save a1} result; puts $result
catch {tech load input/small -noprompt} result; puts $result
catch {save a2} result; puts $result
quit -noprompt
