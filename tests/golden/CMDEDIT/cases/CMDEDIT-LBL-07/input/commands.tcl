# CMDEDIT-LBL-07: goto m1_n5_7#; goto x1/VDD; goto 1_11_14; goto nosuch -nocomplain.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load x} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label VDD} result; puts $result
catch {save x} result; puts $result
catch {load a} result; puts $result
catch {box 50 0 50 0} result; puts $result
catch {getcell x} result; puts $result
catch {select cell x_0} result; puts $result
catch {identify x1} result; puts $result
catch {select clear} result; puts $result
catch {box -10 0 0 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 10 10 20 20} result; puts $result
catch {paint poly} result; puts $result
catch {goto m1_n5_7#} result; puts $result
catch {box values} result; puts $result
catch {goto x1/VDD} result; puts $result
catch {box values} result; puts $result
catch {goto 1_11_14} result; puts $result
catch {box values} result; puts $result
catch {goto nosuch -nocomplain} result; puts $result
catch {box values} result; puts $result
quit -noprompt
