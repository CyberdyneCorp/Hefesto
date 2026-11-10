# DRC-FIND-1: A top cell with 4 error tiles: drc find x5, drc find 3, drc find 9, drc find spacing, drc find width.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 12 0 22 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 40 0 42 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 60 0 62 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 0 40 10 50} result; puts $result
catch {paint m1} result; puts $result
catch {box 11 40 20 50} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
catch {drc find} result; puts $result
catch {box values} result; puts $result
catch {drc find} result; puts $result
catch {box values} result; puts $result
catch {drc find} result; puts $result
catch {box values} result; puts $result
catch {drc find} result; puts $result
catch {box values} result; puts $result
catch {drc find} result; puts $result
catch {box values} result; puts $result
catch {drc find 3} result; puts $result
catch {box values} result; puts $result
catch {drc find 9} result; puts $result
catch {box values} result; puts $result
catch {drc find spacing} result; puts $result
catch {box values} result; puts $result
catch {drc find width} result; puts $result
catch {box values} result; puts $result
catch {drc list find 2} result; puts $result
catch {box values} result; puts $result
quit -noprompt
