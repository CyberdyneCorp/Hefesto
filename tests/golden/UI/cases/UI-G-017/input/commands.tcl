# UI-G-017: logcommands log.txt update; box 0 0 1 1; logcommands.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {logcommands log.txt update} result; puts $result
catch {box 0 0 1 1} result; puts $result
catch {logcommands} result; puts $result
quit -noprompt
