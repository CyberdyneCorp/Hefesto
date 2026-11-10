# CMDEDIT-LBL-08: element add rectangle r1; configure flags persistent; save and reload; element names; element inbox.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {element add rectangle r1 white 0 0 10 10} result; puts $result
catch {element configure r1 flags persistent} result; puts $result
catch {element names} result; puts $result
catch {save a} result; puts $result
catch {load b} result; puts $result
catch {load a} result; puts $result
catch {element names} result; puts $result
catch {box -1 -1 11 11} result; puts $result
catch {element inbox} result; puts $result
quit -noprompt
