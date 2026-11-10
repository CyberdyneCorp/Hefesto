# UI-G-001: getc nonexist and getcell nonexist.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {getc nonexist} result; puts $result
catch {getcell nonexist} result; puts $result
quit -noprompt
