# UI-G-067: feedback add with a numeric third argument, an unknown style and a style long name.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {feedback add x 5 1 2} result; puts $result
catch {feedback count} result; puts $result
catch {feedback add x bogus} result; puts $result
catch {feedback count} result; puts $result
catch {feedback add x solid_highlights} result; puts $result
catch {feedback count} result; puts $result
catch {feedback bogus} result; puts $result
catch {feedback} result; puts $result
quit -noprompt
