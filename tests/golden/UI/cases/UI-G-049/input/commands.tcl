# UI-G-049: macro list per tool and client (Tcl variant of the system startup file).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {macro list} result; puts $result
catch {macro layout list} result; puts $result
catch {macro wiring list} result; puts $result
catch {macro color list} result; puts $result
catch {tool wiring} result; puts $result
catch {macro list} result; puts $result
catch {tool box} result; puts $result
quit -noprompt
