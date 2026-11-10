# CMDEDIT-PAINT-01: Empty cell, box 0 0 10 20; paint poly,metal1; save; undo; save.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 20} result; puts $result
catch {paint poly,metal1} result; puts $result
catch {save p1} result; puts $result
catch {undo} result; puts $result
catch {save p2} result; puts $result
quit -noprompt
