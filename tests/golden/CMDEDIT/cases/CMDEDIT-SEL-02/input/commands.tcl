# CMDEDIT-SEL-02: labels VDD VSS IN; select area labels V*; what -list.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 30 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 2 2 2 2} result; puts $result
catch {label VDD} result; puts $result
catch {box 12 2 12 2} result; puts $result
catch {label VSS} result; puts $result
catch {box 22 2 22 2} result; puts $result
catch {label IN} result; puts $result
catch {box 0 0 30 10} result; puts $result
catch {select area labels V*} result; puts $result
catch {what -list} result; puts $result
quit -noprompt
