# UI-G-005: help, help window, help wizard in the layout window; then in a color window.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {help} result; puts $result
catch {help window} result; puts $result
catch {help wizard} result; puts $result
catch {specialopen color} result; puts $result
catch {help} result; puts $result
catch {help wizard} result; puts $result
quit -noprompt
