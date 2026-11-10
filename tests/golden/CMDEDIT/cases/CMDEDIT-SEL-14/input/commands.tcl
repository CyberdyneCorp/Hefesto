# CMDEDIT-SEL-14: An instance selected; orient for every name, 180v and 9 (Q-CMDEDIT-62).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load leaf} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save leaf} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {select cell leaf_0} result; puts $result
catch {orient 0} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient 90} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient 180} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient 270} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient v} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient 0v} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient 90v} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient 180v} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient 270v} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient h} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient 0h} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient 90h} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient 180h} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient 270h} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient N} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient E} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient S} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient W} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient FN} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient FE} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient FS} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient FW} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {orient 9} result; puts $result
catch {instance orientation leaf_0} result; puts $result
catch {save a} result; puts $result
quit -noprompt
