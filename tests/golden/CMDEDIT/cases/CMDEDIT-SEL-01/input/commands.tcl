# CMDEDIT-SEL-01: metal1 0 0 20 20; select area metal1 under a smaller box; select less area.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 10 10} result; puts $result
catch {select area metal1} result; puts $result
catch {what -list} result; puts $result
catch {select bbox} result; puts $result
catch {select clear} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {select area metal1} result; puts $result
catch {box 5 5 10 10} result; puts $result
catch {select less area metal1} result; puts $result
catch {what -list} result; puts $result
catch {select bbox} result; puts $result
quit -noprompt
