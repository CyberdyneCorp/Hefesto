# CMDEDIT-PAINT-08: Box 0 0 20 20 with a wire entering from each side; the 8 corner combinations; corner n s; a too-wide wire.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load cne} result; puts $result
catch {box 5 -10 8 0} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {corner n e} result; puts $result
catch {save cne} result; puts $result
catch {load cnw} result; puts $result
catch {box 5 -10 8 0} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {corner n w} result; puts $result
catch {save cnw} result; puts $result
catch {load cse} result; puts $result
catch {box 5 20 8 30} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {corner s e} result; puts $result
catch {save cse} result; puts $result
catch {load csw} result; puts $result
catch {box 5 20 8 30} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {corner s w} result; puts $result
catch {save csw} result; puts $result
catch {load cen} result; puts $result
catch {box -10 5 0 8} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {corner e n} result; puts $result
catch {save cen} result; puts $result
catch {load ces} result; puts $result
catch {box -10 5 0 8} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {corner e s} result; puts $result
catch {save ces} result; puts $result
catch {load cwn} result; puts $result
catch {box 20 5 30 8} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {corner w n} result; puts $result
catch {save cwn} result; puts $result
catch {load cws} result; puts $result
catch {box 20 5 30 8} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {corner w s} result; puts $result
catch {save cws} result; puts $result
catch {load cns} result; puts $result
catch {box 5 -10 8 0} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {corner n s} result; puts $result
catch {save cns} result; puts $result
catch {load cwide} result; puts $result
catch {box 0 -10 18 0} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {corner n e} result; puts $result
catch {save cwide} result; puts $result
quit -noprompt
