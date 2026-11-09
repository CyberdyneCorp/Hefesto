# MODEL-G-011: Child with nw/sw/se/ne triangles placed in 8 orientations, flattened.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load child} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {splitpaint nw metal1} result; puts $result
catch {box 20 0 30 10} result; puts $result
catch {splitpaint sw metal1} result; puts $result
catch {box 40 0 50 10} result; puts $result
catch {splitpaint se metal1} result; puts $result
catch {box 60 0 70 10} result; puts $result
catch {splitpaint ne metal1} result; puts $result
catch {save child} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell child 0} result; puts $result
catch {box 0 100 0 100} result; puts $result
catch {getcell child 90} result; puts $result
catch {box 0 200 0 200} result; puts $result
catch {getcell child 180} result; puts $result
catch {box 0 300 0 300} result; puts $result
catch {getcell child 270} result; puts $result
catch {box 0 400 0 400} result; puts $result
catch {getcell child v} result; puts $result
catch {box 0 500 0 500} result; puts $result
catch {getcell child h} result; puts $result
catch {box 0 600 0 600} result; puts $result
catch {getcell child 90v} result; puts $result
catch {box 0 700 0 700} result; puts $result
catch {getcell child 90h} result; puts $result
catch {save top} result; puts $result
catch {load top} result; puts $result
catch {flatten flat} result; puts $result
catch {load flat} result; puts $result
catch {save flat} result; puts $result
quit -noprompt
