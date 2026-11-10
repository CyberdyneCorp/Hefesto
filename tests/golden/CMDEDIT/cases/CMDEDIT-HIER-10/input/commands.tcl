# CMDEDIT-HIER-10: path; path search +lib; addpath x; path cell; locking; locking disable; locking.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {path} result; puts $result
catch {path search +lib} result; puts $result
catch {path search} result; puts $result
catch {addpath x} result; puts $result
catch {path} result; puts $result
catch {path cell} result; puts $result
catch {locking} result; puts $result
catch {locking disable} result; puts $result
catch {locking} result; puts $result
quit -noprompt
