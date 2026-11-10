# CMDEDIT-PAINT-15: Top with a child holding dnwell: select area dnwell; drop pwell; then with the child as edit cell (Q-CMDEDIT-17).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tinydw -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load c} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {paint dnwell} result; puts $result
catch {save c} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell c} result; puts $result
catch {box -5 -5 25 25} result; puts $result
catch {select area dnwell} result; puts $result
catch {drop pwell} result; puts $result
catch {select clear} result; puts $result
catch {select cell c_0} result; puts $result
catch {edit} result; puts $result
catch {select area dnwell} result; puts $result
catch {drop pwell} result; puts $result
catch {writeall force} result; puts $result
quit -noprompt
