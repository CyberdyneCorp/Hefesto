# CMDEDIT-MISC-01: xor of identical cells; overlapping offset metal1; different types; xor -nolabels -novendor (Q-CMDEDIT-94, 95).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load b} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save b} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {xor b} result; puts $result
catch {save a} result; puts $result
catch {load d} result; puts $result
catch {box 5 0 15 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save d} result; puts $result
catch {load c} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {xor d} result; puts $result
catch {save c} result; puts $result
catch {load f} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint poly} result; puts $result
catch {save f} result; puts $result
catch {load e} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label E} result; puts $result
catch {xor -nolabels -novendor f} result; puts $result
catch {save e} result; puts $result
catch {writeall force} result; puts $result
quit -noprompt
