# TECH-G21: scmos-like tech with an explicit 'erase pdc pwell pdiff': paint pdc, erase pwell (Q-TECH-20).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/sl -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint pdc} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint pwell} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {erase pwell} result; puts $result
catch {save a} result; puts $result
quit -noprompt
