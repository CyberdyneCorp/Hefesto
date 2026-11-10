# DRC-ENG-3: A layout at negative coordinates crossing x=0 and y=0 with violations.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box -10 -5 -1 5} result; puts $result
catch {paint m1} result; puts $result
catch {box 1 -5 10 5} result; puts $result
catch {paint m1} result; puts $result
catch {box -5 -20 5 -7} result; puts $result
catch {paint m1} result; puts $result
catch {box -60 -60 -58 -40} result; puts $result
catch {paint m1} result; puts $result
catch {box -100 20 -50 22} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
