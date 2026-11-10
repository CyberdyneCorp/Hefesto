# DRC-STYLE-1: style s variant A,B with variant-restricted rules; drc style sA and sB; printrules and CAP each.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {drc listall style} result; puts $result
catch {drc list style} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 2 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 20 0 23 10} result; puts $result
catch {paint m2} result; puts $result
catch {box 40 0 44 10} result; puts $result
catch {paint poly} result; puts $result
catch {drc style sA} result; puts $result
catch {drc list style} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {drc printrules rulesA.txt} result; puts $result
catch {drc style sB} result; puts $result
catch {drc list style} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
catch {drc printrules rulesB.txt} result; puts $result
catch {drc style s} result; puts $result
catch {drc style x} result; puts $result
quit -noprompt
