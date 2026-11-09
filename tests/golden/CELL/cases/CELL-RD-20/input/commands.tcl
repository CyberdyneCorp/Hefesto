# CELL-RD-20: Properties of every type, reserved bboxes and MASKHINTS (8 and 6 values), GDS_*; magscale 1 2.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/a} result; puts $result
catch {property FIXED_BBOX} result; puts $result
catch {property MASKHINTS_A} result; puts $result
catch {property GDS_START} result; puts $result
catch {property d} result; puts $result
catch {cellname list allcells} result; puts $result
catch {save a_out} result; puts $result
quit -noprompt
