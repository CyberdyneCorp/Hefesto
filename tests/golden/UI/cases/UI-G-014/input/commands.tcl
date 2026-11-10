# UI-G-014: macro wind3d L "level up" in a build without 3-D.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {macro wind3d L "level up"} result; puts $result
quit -noprompt
