# CMDEDIT-SEL-09: copy e 20, 5 7, ne 3 4, to 100 100, 10.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {select area metal1} result; puts $result
catch {copy e 20} result; puts $result
catch {box values} result; puts $result
catch {copy 5 7} result; puts $result
catch {box values} result; puts $result
catch {copy ne 3 4} result; puts $result
catch {box values} result; puts $result
catch {copy to 100 100} result; puts $result
catch {box values} result; puts $result
catch {copy 10} result; puts $result
catch {box values} result; puts $result
catch {save a} result; puts $result
quit -noprompt
