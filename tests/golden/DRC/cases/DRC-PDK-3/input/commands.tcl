# DRC-PDK-3: ihp-sg13g2: a metal1 MOM finger capacitor drawn with box/paint (stand-in for the cap_cmomf generator, design D12) with one finger too close; CAP.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/ihp-sg13g2 -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {scalegrid 1 2} result; puts $result
catch {drc off} result; puts $result
catch {drc euclidean on} result; puts $result
catch {snap internal} result; puts $result
catch {load mom} result; puts $result
catch {box 0 0 1700 120} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 1080 1700 1200} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 120 80 920} result; puts $result
catch {paint metal1} result; puts $result
catch {box 160 280 240 1080} result; puts $result
catch {paint metal1} result; puts $result
catch {box 320 120 400 920} result; puts $result
catch {paint metal1} result; puts $result
catch {box 480 280 560 1080} result; puts $result
catch {paint metal1} result; puts $result
catch {box 640 120 720 920} result; puts $result
catch {paint metal1} result; puts $result
catch {box 800 280 880 1080} result; puts $result
catch {paint metal1} result; puts $result
catch {box 960 120 1040 920} result; puts $result
catch {paint metal1} result; puts $result
catch {box 1120 280 1200 1080} result; puts $result
catch {paint metal1} result; puts $result
catch {box 1280 120 1360 920} result; puts $result
catch {paint metal1} result; puts $result
catch {box 1440 280 1520 1080} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 1700 120} result; puts $result
catch {paint metal2} result; puts $result
catch {box 0 1080 1700 1200} result; puts $result
catch {paint metal2} result; puts $result
catch {box 40 20 140 100} result; puts $result
catch {paint via1} result; puts $result
catch {box 40 1100 140 1180} result; puts $result
catch {paint via1} result; puts $result
catch {box 1540 120 1620 920} result; puts $result
catch {paint metal1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save mom} result; puts $result
quit -noprompt
