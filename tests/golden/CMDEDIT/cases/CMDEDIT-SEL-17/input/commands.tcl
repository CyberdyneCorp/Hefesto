# CMDEDIT-SEL-17: Paint, a label and an instance: what, what -list, what -listall; a cell; nothing.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load leaf} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save leaf} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label L} result; puts $result
catch {box 20 0 20 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {box -5 -5 40 15} result; puts $result
catch {select area} result; puts $result
catch {what} result; puts $result
catch {what -list} result; puts $result
catch {what -listall} result; puts $result
catch {select cell leaf_0} result; puts $result
catch {what} result; puts $result
catch {what -list} result; puts $result
catch {select clear} result; puts $result
catch {what} result; puts $result
catch {what -list} result; puts $result
quit -noprompt
