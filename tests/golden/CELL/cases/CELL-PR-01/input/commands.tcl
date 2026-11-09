# CELL-PR-01: Parent mid/top uses inv from ../lib; a second parent other/top2 references ../lib too.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/mid/top} result; puts $result
catch {load input/other/top2} result; puts $result
catch {cellname list allcells} result; puts $result
catch {load top} result; puts $result
catch {save top_out} result; puts $result
quit -noprompt
