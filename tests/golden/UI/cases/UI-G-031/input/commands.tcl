# UI-G-031: openwindow; closewindow; openwindow; windownames all.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {openwindow} result; puts $result
catch {closewindow} result; puts $result
catch {openwindow} result; puts $result
catch {windownames all} result; puts $result
quit -noprompt
