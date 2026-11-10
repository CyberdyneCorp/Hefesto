# CMDEDIT-HIER-03: dump blk with the box ll at 50 50; dump blk 90v.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load blk} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label B} result; puts $result
catch {save blk} result; puts $result
catch {load top} result; puts $result
catch {box 50 50 50 50} result; puts $result
catch {dump blk} result; puts $result
catch {what -list} result; puts $result
catch {box 100 100 100 100} result; puts $result
catch {dump blk 90v} result; puts $result
catch {what -list} result; puts $result
catch {save top} result; puts $result
quit -noprompt
