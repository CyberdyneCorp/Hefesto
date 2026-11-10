# DRC-FLAT-1: drc *flatcheck on the hierarchical design of DRC-ENG-4.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load child} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint m1} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell child} result; puts $result
catch {box 12 0 22 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 40 0 42 10} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {drc *flatcheck} result; puts $result
quit -noprompt
