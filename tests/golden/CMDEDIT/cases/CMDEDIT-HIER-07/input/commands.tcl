# CMDEDIT-HIER-07: expand, expand surround, unexpand, expand toggle, xload top; what -list of a box selection after each.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save a} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell a} result; puts $result
catch {box 20 0 30 10} result; puts $result
catch {paint metal2} result; puts $result
catch {box -5 -5 35 15} result; puts $result
catch {expand} result; puts $result
catch {select area} result; puts $result
catch {what -list} result; puts $result
catch {select clear} result; puts $result
catch {box -5 -5 35 15} result; puts $result
catch {expand surround} result; puts $result
catch {select area} result; puts $result
catch {what -list} result; puts $result
catch {select clear} result; puts $result
catch {box -5 -5 35 15} result; puts $result
catch {unexpand} result; puts $result
catch {select area} result; puts $result
catch {what -list} result; puts $result
catch {select clear} result; puts $result
catch {box -5 -5 35 15} result; puts $result
catch {expand toggle} result; puts $result
catch {select area} result; puts $result
catch {what -list} result; puts $result
catch {select clear} result; puts $result
catch {box -5 -5 35 15} result; puts $result
catch {xload top} result; puts $result
catch {select area} result; puts $result
catch {what -list} result; puts $result
catch {select clear} result; puts $result
quit -noprompt
