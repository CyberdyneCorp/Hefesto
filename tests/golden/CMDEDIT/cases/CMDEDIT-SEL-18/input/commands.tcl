# CMDEDIT-SEL-18: identify buf1 on an instance; a duplicate; identify a/b; nothing selected.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load leaf} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save leaf} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {box 20 0 20 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {select cell leaf_0} result; puts $result
catch {identify buf1} result; puts $result
catch {select cell leaf_1} result; puts $result
catch {identify buf1} result; puts $result
catch {identify a/b} result; puts $result
catch {select clear} result; puts $result
catch {identify x} result; puts $result
catch {save a} result; puts $result
quit -noprompt
