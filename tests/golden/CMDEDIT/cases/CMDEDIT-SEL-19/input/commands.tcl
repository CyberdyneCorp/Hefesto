# CMDEDIT-SEL-19: select pick; select move 20 0; select keep.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {select area} result; puts $result
catch {select pick} result; puts $result
catch {select move 20 0} result; puts $result
catch {select keep} result; puts $result
catch {save a} result; puts $result
quit -noprompt
