# MODEL-G-032: A .mag with a locked use (*inv_0); load and save (instance lock crashed Magic in capture: exit 139; move/delete not authored).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/top} result; puts $result
catch {cellname list modified} result; puts $result
catch {save top_out} result; puts $result
quit -noprompt
