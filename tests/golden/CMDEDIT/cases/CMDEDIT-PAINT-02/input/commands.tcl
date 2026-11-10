# CMDEDIT-PAINT-02: paint *,-metal2; paint -poly,metal1; paint metal1,connect; paint foo, labels, subcell.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load p1} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint *,-metal2} result; puts $result
catch {save p1} result; puts $result
catch {load p2} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint -poly,metal1} result; puts $result
catch {save p2} result; puts $result
catch {load p3} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1,connect} result; puts $result
catch {paint foo} result; puts $result
catch {paint labels} result; puts $result
catch {paint subcell} result; puts $result
catch {save p3} result; puts $result
quit -noprompt
