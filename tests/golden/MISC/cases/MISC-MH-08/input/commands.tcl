# MISC-MH-08: exception X width rule; a 2-unit wire per cell with hints touching it from the left, right, above and below, straddling hints, an L-shaped hint and far hints (Q-MISC-13); drc listall why and count.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/t -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load H0} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load He} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X -10 -10 30 10} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Ha} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X -10 -10 30 1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Hb} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X -10 -10 30 0} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Hc} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X -10 2 30 10} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Hd} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X -10 -10 5 0 -10 0 30 2 -10 2 31 10} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Hd2} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X -10 -10 5 0 -10 0 30 10} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Hf} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X -10 1 30 10} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Ve} result; puts $result
catch {box 0 0 2 20} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X -10 -10 10 30} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Va} result; puts $result
catch {box 0 0 2 20} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X -10 -10 0 30} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Vb} result; puts $result
catch {box 0 0 2 20} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X 2 -10 10 30} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Vc} result; puts $result
catch {box 0 0 2 20} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X 1 -10 10 30} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Vd} result; puts $result
catch {box 0 0 2 20} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X -10 -10 1 30} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Vf} result; puts $result
catch {box 0 0 2 20} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X 0 -10 1 30} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Vh} result; puts $result
catch {box 0 0 2 20} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X 0 5 1 10} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Hg} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X 5 -5 15 5} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Hh} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X 5 -5 15 5 -100 0 -90 10} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Hi} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X 5 -5 15 5 100 0 110 10} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
catch {load Hj} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X -10 -5 30 5 -100 0 -90 10} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc listall count} result; puts $result
quit -noprompt
