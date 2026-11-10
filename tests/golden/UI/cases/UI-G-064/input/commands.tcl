# UI-G-064: feedback add with a square and a triangle polygon; count; areas through find.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {feedback add "p" solid 0 0 10 0 10 10 0 10} result; puts $result
catch {feedback add "t" pale 0 0 10 0 0 10} result; puts $result
catch {feedback count} result; puts $result
catch {feedback find} result; puts $result
catch {box values} result; puts $result
catch {feedback find} result; puts $result
catch {box values} result; puts $result
catch {feedback find} result; puts $result
catch {box values} result; puts $result
quit -noprompt
