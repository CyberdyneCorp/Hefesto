# DRC-SCALE-1: scalefactor 10: spacing 25, maxwidth 25 bend_illegal, area 1050 20; printrules, *halo, *stepsize, CAP.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 3 3} result; puts $result
catch {paint m1} result; puts $result
catch {box 5 0 8 3} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
catch {drc *halo} result; puts $result
catch {drc *stepsize} result; puts $result
catch {drc printrules rules.txt} result; puts $result
quit -noprompt
