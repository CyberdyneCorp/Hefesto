# MISC-MH-01: property MASKHINTS_SRAM with two overlapping rectangles; save; read back a file with a hint value.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/t -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_SRAM 0 0 10 10 5 5 15 15} result; puts $result
catch {property MASKHINTS_SRAM} result; puts $result
catch {save a} result; puts $result
catch {load input/r} result; puts $result
catch {property MASKHINTS_SRAM} result; puts $result
catch {property} result; puts $result
quit -noprompt
