# CMDEDIT-MISC-02: see no metal1; select visible; see allSame; select visible.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 5 5} result; puts $result
catch {paint poly} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {see no metal1} result; puts $result
catch {select visible} result; puts $result
catch {what -list} result; puts $result
catch {select clear} result; puts $result
catch {see allSame} result; puts $result
catch {select visible} result; puts $result
catch {what -list} result; puts $result
quit -noprompt
