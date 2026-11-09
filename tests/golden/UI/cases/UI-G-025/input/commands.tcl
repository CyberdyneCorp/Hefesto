# UI-G-025: view; view get; zoom 2; view get; zoom .5; view get.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {view} result; puts $result
catch {view get} result; puts $result
catch {zoom 2} result; puts $result
catch {view get} result; puts $result
catch {zoom .5} result; puts $result
catch {view get} result; puts $result
quit -noprompt
