# DRC-EXT-2: extend nfet poly 3 exact_width; gate length 4.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint ndiff} result; puts $result
catch {box 3 -3 7 13} result; puts $result
catch {paint poly} result; puts $result
catch {box 20 0 30 10} result; puts $result
catch {paint ndiff} result; puts $result
catch {box 24 -3 27 13} result; puts $result
catch {paint poly} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
