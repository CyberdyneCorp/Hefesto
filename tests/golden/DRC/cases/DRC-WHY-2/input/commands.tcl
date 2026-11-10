# DRC-WHY-2: drc list why and drc listall why with units microns and the default units.
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
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
catch {select top cell} result; puts $result
catch {drc list why} result; puts $result
catch {drc listall why} result; puts $result
catch {units microns} result; puts $result
catch {drc list why} result; puts $result
catch {drc listall why} result; puts $result
catch {units lambda} result; puts $result
catch {drc listall why} result; puts $result
catch {units default} result; puts $result
catch {drc listall why} result; puts $result
quit -noprompt
