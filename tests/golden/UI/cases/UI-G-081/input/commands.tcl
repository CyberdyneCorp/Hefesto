# UI-G-081: *coord with a box set; *winddebug twice; *files.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {*coord} result; puts $result
catch {*winddebug} result; puts $result
catch {*winddebug} result; puts $result
catch {*files} result; puts $result
quit -noprompt
