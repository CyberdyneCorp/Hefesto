# CMDEDIT-SEL-06: dnwell + nwell; select area dnwell; select intersection ~nwell.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tinydw -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 30 30} result; puts $result
catch {paint dnwell} result; puts $result
catch {box 10 10 40 40} result; puts $result
catch {paint nwell} result; puts $result
catch {box -5 -5 50 50} result; puts $result
catch {select area dnwell} result; puts $result
catch {select intersection ~nwell} result; puts $result
catch {what -list} result; puts $result
catch {select bbox} result; puts $result
quit -noprompt
