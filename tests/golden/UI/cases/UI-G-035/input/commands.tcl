# UI-G-035: specialopen usage forms (hidden, unknown, short coordinates) and a layout window by prefix; the color window is ui-color-window (M2).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {specialopen *window} result; puts $result
catch {specialopen foo} result; puts $result
catch {specialopen} result; puts $result
catch {specialopen 1 2 layout} result; puts $result
catch {specialopen la c} result; puts $result
catch {windownames all} result; puts $result
quit -noprompt
