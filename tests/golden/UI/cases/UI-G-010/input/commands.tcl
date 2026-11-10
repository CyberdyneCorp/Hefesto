# UI-G-010: macro z, macro z "", macro z.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {macro z} result; puts $result
catch {macro z ""} result; puts $result
catch {macro z} result; puts $result
quit -noprompt
