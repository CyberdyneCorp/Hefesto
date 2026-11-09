# TECH-G22: m2c over pc (a); pc,m2c on empty space (b); erase pc,m2c (c).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint pc} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint m2c} result; puts $result
catch {save a} result; puts $result
catch {load b} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint pc,m2c} result; puts $result
catch {save b} result; puts $result
catch {load c} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint pc,m2c} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {erase pc,m2c} result; puts $result
catch {save c} result; puts $result
quit -noprompt
