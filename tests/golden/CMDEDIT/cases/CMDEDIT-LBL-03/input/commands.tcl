# CMDEDIT-LBL-03: Two selected labels: each setlabel query and some sets; setlabel -default size 2 then label (Q-CMDEDIT-90, 91).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 20 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 2 2 2 2} result; puts $result
catch {label A} result; puts $result
catch {box 12 2 12 2} result; puts $result
catch {label B} result; puts $result
catch {box 0 0 20 10} result; puts $result
catch {select area labels} result; puts $result
catch {setlabel text} result; puts $result
catch {setlabel font} result; puts $result
catch {setlabel justify} result; puts $result
catch {setlabel size} result; puts $result
catch {setlabel rotate} result; puts $result
catch {setlabel offset} result; puts $result
catch {setlabel layer} result; puts $result
catch {setlabel sticky} result; puts $result
catch {setlabel justify ne} result; puts $result
catch {setlabel rotate 90} result; puts $result
catch {setlabel text Z} result; puts $result
catch {setlabel text} result; puts $result
catch {setlabel -default size 2} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label N} result; puts $result
catch {save a} result; puts $result
quit -noprompt
