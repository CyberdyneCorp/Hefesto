# CMDEDIT-SEL-05: Nested t > a > b, expanded; select cell at a point x3; select cell a_0/b_0; select top cell.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load b} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save b} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell b} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {paint metal2} result; puts $result
catch {save a} result; puts $result
catch {load t} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell a} result; puts $result
catch {box -5 -5 25 25} result; puts $result
catch {expand} result; puts $result
catch {select cell at 5 5} result; puts $result
catch {select bbox} result; puts $result
catch {select cell at 5 5} result; puts $result
catch {select bbox} result; puts $result
catch {select cell at 5 5} result; puts $result
catch {select bbox} result; puts $result
catch {select cell a_0/b_0} result; puts $result
catch {select bbox} result; puts $result
catch {box values} result; puts $result
catch {select top cell at 5 5} result; puts $result
catch {select bbox} result; puts $result
quit -noprompt
