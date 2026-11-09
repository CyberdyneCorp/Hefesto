# TECH-G11: tech sections format 34, format 33, formatX 40, format abc, 'a b c' and none (extract query not authored).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/t34 -noprompt} result; puts $result
catch {tech name} result; puts $result
catch {tech load input/t33 -noprompt} result; puts $result
catch {tech name} result; puts $result
catch {tech load input/tx40 -noprompt} result; puts $result
catch {tech name} result; puts $result
catch {tech load input/tabc -noprompt} result; puts $result
catch {tech name} result; puts $result
catch {tech load input/tabc3 -noprompt} result; puts $result
catch {tech name} result; puts $result
catch {tech load input/tnone -noprompt} result; puts $result
catch {tech name} result; puts $result
quit -noprompt
