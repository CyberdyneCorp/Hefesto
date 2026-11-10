# CMDEDIT-HIER-09: save adder from (UNNAMED); writeall force; flush a -noprompt.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save adder} result; puts $result
catch {load b} result; puts $result
catch {box 0 0 5 5} result; puts $result
catch {paint metal1} result; puts $result
catch {writeall force} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 5 5} result; puts $result
catch {paint poly} result; puts $result
catch {flush a -noprompt} result; puts $result
catch {cellname list modified} result; puts $result
quit -noprompt
