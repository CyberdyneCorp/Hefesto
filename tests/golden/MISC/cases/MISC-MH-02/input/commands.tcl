# MISC-MH-02: A hint 0 0 9 9 read at the default grid, then a file at magscale 1 2 with the same hint (the grid becomes 1/2), then scalegrid 1 2; the hint values after each step; writeall.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/t -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load input/a} result; puts $result
catch {property MASKHINTS_X} result; puts $result
catch {load input/b} result; puts $result
catch {property MASKHINTS_X} result; puts $result
catch {load input/a} result; puts $result
catch {property MASKHINTS_X} result; puts $result
catch {scalegrid 1 2} result; puts $result
catch {property MASKHINTS_X} result; puts $result
catch {load input/b} result; puts $result
catch {property MASKHINTS_X} result; puts $result
catch {writeall force} result; puts $result
quit -noprompt
