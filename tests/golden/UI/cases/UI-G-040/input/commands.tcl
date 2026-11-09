# UI-G-040: grid multiple 300; grid multiple.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {grid multiple 300} result; puts $result
catch {grid multiple} result; puts $result
quit -noprompt
