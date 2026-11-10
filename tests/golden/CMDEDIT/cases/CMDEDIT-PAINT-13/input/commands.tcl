# CMDEDIT-PAINT-13: box 0 0 10 10; splitpaint D metal1 poly for every direction (Q-CMDEDIT-21).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load sne} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {splitpaint ne metal1 poly} result; puts $result
catch {save sne} result; puts $result
catch {load snw} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {splitpaint nw metal1 poly} result; puts $result
catch {save snw} result; puts $result
catch {load sse} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {splitpaint se metal1 poly} result; puts $result
catch {save sse} result; puts $result
catch {load ssw} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {splitpaint sw metal1 poly} result; puts $result
catch {save ssw} result; puts $result
catch {load sn} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {splitpaint n metal1 poly} result; puts $result
catch {save sn} result; puts $result
catch {load se} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {splitpaint e metal1 poly} result; puts $result
catch {save se} result; puts $result
catch {load ss} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {splitpaint s metal1 poly} result; puts $result
catch {save ss} result; puts $result
catch {load sw} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {splitpaint w metal1 poly} result; puts $result
catch {save sw} result; puts $result
catch {load scenter} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {splitpaint center metal1 poly} result; puts $result
catch {save scenter} result; puts $result
quit -noprompt
