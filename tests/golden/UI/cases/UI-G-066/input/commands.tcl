# UI-G-066: feedback clear b; count; clear; count.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {feedback add "a"} result; puts $result
catch {feedback add "b"} result; puts $result
catch {feedback add "ab"} result; puts $result
catch {feedback add "c"} result; puts $result
catch {feedback clear b} result; puts $result
catch {feedback count} result; puts $result
catch {feedback find} result; puts $result
catch {feedback clear} result; puts $result
catch {feedback count} result; puts $result
quit -noprompt
