# CMDEDIT-LBL-02: label VDD n -metal2; label A 0 2 45 1 1 sw metal1; label "a b"; label x/y.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label VDD n -metal2} result; puts $result
catch {label A 0 2 45 1 1 sw metal1} result; puts $result
catch {label "a b"} result; puts $result
catch {label x/y} result; puts $result
catch {save a} result; puts $result
quit -noprompt
