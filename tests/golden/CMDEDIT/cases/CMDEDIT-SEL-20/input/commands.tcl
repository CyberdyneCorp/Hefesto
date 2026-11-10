# CMDEDIT-SEL-20: Labeled net A-B; select short A B; select bbox; select save sel1.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 30 4} result; puts $result
catch {paint metal1} result; puts $result
catch {box 1 2 1 2} result; puts $result
catch {label A} result; puts $result
catch {box 29 2 29 2} result; puts $result
catch {label B} result; puts $result
catch {select short A B} result; puts $result
catch {select bbox} result; puts $result
catch {what -list} result; puts $result
catch {select save sel1} result; puts $result
quit -noprompt
