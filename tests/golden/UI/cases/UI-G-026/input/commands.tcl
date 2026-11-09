# UI-G-026: zoom 0, zoom 20, zoom 25, zoom -1, zoom.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {zoom 0} result; puts $result
catch {zoom 20} result; puts $result
catch {zoom 25} result; puts $result
catch {zoom -1} result; puts $result
catch {zoom} result; puts $result
catch {view get} result; puts $result
quit -noprompt
