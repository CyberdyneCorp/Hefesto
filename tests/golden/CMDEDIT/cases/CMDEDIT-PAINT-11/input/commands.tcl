# CMDEDIT-PAINT-11: metal1 and metal2 overlap; contact m2c; contact erase m2c; contact metal1.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 6 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 3 0 10 10} result; puts $result
catch {paint metal2} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {contact m2c} result; puts $result
catch {save c1} result; puts $result
catch {contact erase m2c} result; puts $result
catch {save c2} result; puts $result
catch {contact metal1} result; puts $result
quit -noprompt
