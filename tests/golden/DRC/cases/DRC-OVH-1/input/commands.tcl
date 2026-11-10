# DRC-OVH-1: overhang poly nfet 2; endcap 1 on one side; poly absent on one side.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint ndiff} result; puts $result
catch {box 4 -1 6 12} result; puts $result
catch {paint poly} result; puts $result
catch {box 20 0 30 10} result; puts $result
catch {paint ndiff} result; puts $result
catch {box 24 0 26 12} result; puts $result
catch {paint poly} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
