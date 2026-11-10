# UI-G-033: Edit into leaf inside top (m2 edit); a second window on nand2; windowcaption in each window.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {load input/top} result; puts $result
catch {windowcaption} result; puts $result
catch {select cell leaf_0} result; puts $result
catch {edit} result; puts $result
catch {windowcaption} result; puts $result
catch {openwindow input/nand2} result; puts $result
catch {windowcaption} result; puts $result
catch {windownames all} result; puts $result
quit -noprompt
