# CMDEDIT-MISC-03: feedback add over the box and a triangle; count; why; save; clear x.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {feedback add "x"} result; puts $result
catch {feedback add "p" solid 0 0 10 0 0 10} result; puts $result
catch {feedback count} result; puts $result
catch {feedback why} result; puts $result
catch {feedback save f.tcl} result; puts $result
catch {feedback clear x} result; puts $result
catch {feedback count} result; puts $result
quit -noprompt
