# UI-G-039: grid 10 20 5 0; grid box; grid state; grid; grid state; grid 0; grid -1; grid 1 2 3.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {grid 10 20 5 0} result; puts $result
catch {grid box} result; puts $result
catch {grid state} result; puts $result
catch {grid} result; puts $result
catch {grid state} result; puts $result
catch {grid 0} result; puts $result
catch {grid -1} result; puts $result
catch {grid 1 2 3} result; puts $result
quit -noprompt
