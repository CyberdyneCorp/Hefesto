# UI-G-027: scroll r .1 w; scroll n; scroll e 10; scroll x 1 w.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {scroll r .1 w} result; puts $result
catch {view get} result; puts $result
catch {scroll n} result; puts $result
catch {view get} result; puts $result
catch {scroll e 10} result; puts $result
catch {view get} result; puts $result
catch {scroll x 1 w} result; puts $result
catch {view get} result; puts $result
quit -noprompt
