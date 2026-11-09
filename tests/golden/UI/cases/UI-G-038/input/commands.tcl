# UI-G-038: see no metal1; see allSame; see no allSame; see no a b c.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {see no metal1} result; puts $result
catch {see allSame} result; puts $result
catch {see no allSame} result; puts $result
catch {see no a b c} result; puts $result
catch {see} result; puts $result
quit -noprompt
