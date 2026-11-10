# DRC-OVL-1: Default illegal overlap: poly in the parent over ndiff in a child; CAP (error_s, why 2); printrules.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load child} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint ndiff} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell child} result; puts $result
catch {box 4 -3 6 13} result; puts $result
catch {paint poly} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {writeall force} result; puts $result
catch {drc printrules rules.txt} result; puts $result
quit -noprompt
