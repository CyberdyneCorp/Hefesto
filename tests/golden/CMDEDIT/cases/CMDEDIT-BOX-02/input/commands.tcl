# CMDEDIT-BOX-02: grow center 2, shrink n 30, move ne 3, move n 0.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box values 10 10 20 20} result; puts $result
catch {box grow center 2} result; puts $result
catch {box values} result; puts $result
catch {box shrink n 30} result; puts $result
catch {box values} result; puts $result
catch {box move ne 3} result; puts $result
catch {box values} result; puts $result
catch {box move n 0} result; puts $result
catch {box values} result; puts $result
quit -noprompt
