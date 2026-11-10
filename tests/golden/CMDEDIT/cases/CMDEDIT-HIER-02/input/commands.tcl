# CMDEDIT-HIER-02: getcell pad child P; getcell c 90; getcell c child ur parent ll; each orientation keyword.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load pad} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {paint metal1} result; puts $result
catch {box 10 10 10 10} result; puts $result
catch {label P} result; puts $result
catch {save pad} result; puts $result
catch {load c} result; puts $result
catch {box 0 0 10 20} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label Q} result; puts $result
catch {save c} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell pad child P} result; puts $result
catch {box 100 0 100 0} result; puts $result
catch {getcell c 90} result; puts $result
catch {box 200 0 200 0} result; puts $result
catch {getcell c child ur parent ll} result; puts $result
catch {box 300 0 300 0} result; puts $result
catch {getcell c 0} result; puts $result
catch {box 340 0 340 0} result; puts $result
catch {getcell c 90} result; puts $result
catch {box 380 0 380 0} result; puts $result
catch {getcell c 180} result; puts $result
catch {box 420 0 420 0} result; puts $result
catch {getcell c 270} result; puts $result
catch {box 460 0 460 0} result; puts $result
catch {getcell c v} result; puts $result
catch {box 500 0 500 0} result; puts $result
catch {getcell c h} result; puts $result
catch {box 540 0 540 0} result; puts $result
catch {getcell c 90v} result; puts $result
catch {box 580 0 580 0} result; puts $result
catch {getcell c 90h} result; puts $result
catch {box values} result; puts $result
catch {save top} result; puts $result
quit -noprompt
