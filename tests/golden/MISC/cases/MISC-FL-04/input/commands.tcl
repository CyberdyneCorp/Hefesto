# MISC-FL-04: setlabel fontlist; setlabel font FreeSans.pt3 0.58 twice; font index queries; a missing font file; setlabel -default font/rotate/justify then label (Q-MISC-26).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {setlabel fontlist} result; puts $result
catch {setlabel font FreeSans.pt3 0.58} result; puts $result
catch {setlabel font FreeSans.pt3 0.58} result; puts $result
catch {setlabel fontlist} result; puts $result
catch {setlabel font 1} result; puts $result
catch {setlabel font -1} result; puts $result
catch {setlabel font 9} result; puts $result
catch {setlabel font nosuch.pt3} result; puts $result
catch {setlabel -default font} result; puts $result
catch {setlabel -default justify} result; puts $result
catch {setlabel -default font FreeMono} result; puts $result
catch {setlabel -default rotate 45} result; puts $result
catch {setlabel -default justify ne} result; puts $result
catch {setlabel -default font} result; puts $result
catch {setlabel -default rotate} result; puts $result
catch {setlabel -default justify} result; puts $result
catch {setlabel nosuch} result; puts $result
catch {setlabel} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label D} result; puts $result
catch {save a} result; puts $result
quit -noprompt
