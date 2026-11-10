# UI-G-063: feedback add over the box, a line and a point; count; find three times and find 5; why.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {feedback add "a"} result; puts $result
catch {feedback add "b" medium 0 0 10 10} result; puts $result
catch {feedback add "c" outline 1 1} result; puts $result
catch {feedback count} result; puts $result
catch {feedback find} result; puts $result
catch {box values} result; puts $result
catch {feedback find} result; puts $result
catch {box values} result; puts $result
catch {feedback find 5} result; puts $result
catch {box values} result; puts $result
catch {feedback why} result; puts $result
quit -noprompt
