# MODEL-G-048: property GDS_FILE x.gds; cellname rename; remove the property; cellname readonly.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {property GDS_FILE x.gds} result; puts $result
catch {cellname readonly a} result; puts $result
catch {cellname rename a b} result; puts $result
catch {property GDS_FILE} result; puts $result
catch {property GDS_FILE {}} result; puts $result
catch {cellname readonly a} result; puts $result
catch {cellname rename a b} result; puts $result
quit -noprompt
