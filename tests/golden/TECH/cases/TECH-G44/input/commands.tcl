# TECH-G44: scmos.tech; tech drc width/spacing/surround/directional queries, a bad type and no option.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {tech drc width m1} result; puts $result
catch {tech drc spacing m1} result; puts $result
catch {tech drc spacing m1 m2} result; puts $result
catch {tech drc surround m2c m1} result; puts $result
catch {tech drc direc m2c m1} result; puts $result
catch {tech drc surround m1} result; puts $result
catch {tech drc width nosuch} result; puts $result
catch {tech drc} result; puts $result
quit -noprompt
