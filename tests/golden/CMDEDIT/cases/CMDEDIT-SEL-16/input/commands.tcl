# CMDEDIT-SEL-16: Labels D0 and A[0][0] selected; array 4 1, 1 3, 2 2.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load l1} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 2 2 2 2} result; puts $result
catch {label D0} result; puts $result
catch {box 6 6 6 6} result; puts $result
catch {label {A[0][0]}} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {select area labels} result; puts $result
catch {array 4 1} result; puts $result
catch {save l1} result; puts $result
catch {load l2} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 2 2 2 2} result; puts $result
catch {label D0} result; puts $result
catch {box 6 6 6 6} result; puts $result
catch {label {A[0][0]}} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {select area labels} result; puts $result
catch {array 1 3} result; puts $result
catch {save l2} result; puts $result
catch {load l3} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 2 2 2 2} result; puts $result
catch {label D0} result; puts $result
catch {box 6 6 6 6} result; puts $result
catch {label {A[0][0]}} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {select area labels} result; puts $result
catch {array 2 2} result; puts $result
catch {save l3} result; puts $result
quit -noprompt
