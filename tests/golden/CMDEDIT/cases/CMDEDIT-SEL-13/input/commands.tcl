# CMDEDIT-SEL-13: metal1, a label and an instance selected: clockwise, 180, rotate -90, 90 -orig, 45, sideways, upsidedown (Q-CMDEDIT-61).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load leaf} result; puts $result
catch {box 0 0 5 5} result; puts $result
catch {paint poly} result; puts $result
catch {save leaf} result; puts $result
catch {load a} result; puts $result
catch {box 10 10 30 20} result; puts $result
catch {paint metal1} result; puts $result
catch {box 15 15 15 15} result; puts $result
catch {label L} result; puts $result
catch {box 40 0 40 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {box 0 0 60 30} result; puts $result
catch {select area} result; puts $result
catch {clockwise} result; puts $result
catch {box values} result; puts $result
catch {save r1} result; puts $result
catch {clockwise 180} result; puts $result
catch {box values} result; puts $result
catch {save r2} result; puts $result
catch {rotate -90} result; puts $result
catch {box values} result; puts $result
catch {save r3} result; puts $result
catch {clockwise 90 -orig} result; puts $result
catch {box values} result; puts $result
catch {save r4} result; puts $result
catch {clockwise 45} result; puts $result
catch {box values} result; puts $result
catch {save r5} result; puts $result
catch {sideways} result; puts $result
catch {box values} result; puts $result
catch {save r6} result; puts $result
catch {upsidedown} result; puts $result
catch {box values} result; puts $result
catch {save r7} result; puts $result
quit -noprompt
