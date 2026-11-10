# CMDEDIT-SEL-03: metal1-m2c-metal2 net; select at 5 5 repeated (cycling).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 4 4 6 6} result; puts $result
catch {paint m2c} result; puts $result
catch {box 0 0 20 6} result; puts $result
catch {paint metal2} result; puts $result
catch {select at 5 5} result; puts $result
catch {select bbox} result; puts $result
catch {what -list} result; puts $result
catch {select at 5 5} result; puts $result
catch {select at 5 5} result; puts $result
catch {select bbox} result; puts $result
catch {what -list} result; puts $result
catch {select at 5 5} result; puts $result
catch {select bbox} result; puts $result
quit -noprompt
