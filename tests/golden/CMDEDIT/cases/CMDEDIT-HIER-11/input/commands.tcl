# CMDEDIT-HIER-11: archive writeall arch.mag; archive read into a fresh cell; archive write (Q-CMDEDIT-34).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save a} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell a} result; puts $result
catch {box 20 0 30 10} result; puts $result
catch {paint metal2} result; puts $result
catch {archive writeall arch.mag} result; puts $result
catch {load z} result; puts $result
catch {archive read arch.mag} result; puts $result
catch {cellname list allcells} result; puts $result
catch {archive write} result; puts $result
quit -noprompt
