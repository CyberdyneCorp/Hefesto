# CMDEDIT-HIER-04: load newcell; load nosuch -fail; load a[1]; load x scaled 2.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load newcell} result; puts $result
catch {cellname list allcells} result; puts $result
catch {load nosuch -fail} result; puts $result
catch {cellname list allcells} result; puts $result
catch {load {a[1]}} result; puts $result
catch {cellname list allcells} result; puts $result
catch {load x scaled 2} result; puts $result
catch {cellname list allcells} result; puts $result
quit -noprompt
