# UI-G-036: Two windows on top; expand in one; what -list of a box selection, then select cell at the child.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {load input/top} result; puts $result
catch {openwindow input/top} result; puts $result
catch {windownames all} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {expand} result; puts $result
catch {select area} result; puts $result
catch {what -list} result; puts $result
catch {select clear} result; puts $result
catch {select cell at 5 5} result; puts $result
catch {what -list} result; puts $result
quit -noprompt
