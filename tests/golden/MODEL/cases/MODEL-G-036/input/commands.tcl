# MODEL-G-036: cellname rename a b with b existing; rename a vendor-GDS cell without and with -force.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 1 1} result; puts $result
catch {paint metal1} result; puts $result
catch {save a} result; puts $result
catch {load b} result; puts $result
catch {save b} result; puts $result
catch {cellname rename a b} result; puts $result
catch {load input/v} result; puts $result
catch {cellname rename v w} result; puts $result
catch {cellname rename v w -force} result; puts $result
catch {load w} result; puts $result
catch {property} result; puts $result
catch {save w} result; puts $result
quit -noprompt
