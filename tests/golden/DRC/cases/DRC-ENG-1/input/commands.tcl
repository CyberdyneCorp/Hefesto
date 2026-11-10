# DRC-ENG-1: top > mid > leaf; paint in the leaf after building; the checkpaint sections of all three before any check.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load leaf} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint m1} result; puts $result
catch {load mid} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {load top} result; puts $result
catch {box 100 50 100 50} result; puts $result
catch {getcell mid} result; puts $result
catch {writeall force} result; puts $result
catch {load leaf} result; puts $result
catch {box 20 0 30 5} result; puts $result
catch {paint m1} result; puts $result
catch {writeall force} result; puts $result
quit -noprompt
