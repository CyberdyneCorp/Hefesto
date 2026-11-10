# DRC-EXC-5: exception SRAM spacing rule; hint in a subcell; violations in the parent interaction area inside and outside the child's hint.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load child} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 30 0 40 10} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_SRAM 0 0 20 10} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell child} result; puts $result
catch {box 12 0 18 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 42 0 50 10} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {writeall force} result; puts $result
quit -noprompt
