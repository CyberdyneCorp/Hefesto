# DRC-PDK-2: gf180mcuD: a row of abutted gf180mcu_fd_sc_mcu7t5v0 cells (inv_1, nand2_1, dffq_1); CAP.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/gf180mcuD -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {scalegrid 1 10} result; puts $result
catch {drc off} result; puts $result
catch {drc euclidean on} result; puts $result
catch {snap internal} result; puts $result
catch {load input/gf180mcu_fd_sc_mcu7t5v0__inv_1} result; puts $result
catch {load input/gf180mcu_fd_sc_mcu7t5v0__nand2_1} result; puts $result
catch {load input/gf180mcu_fd_sc_mcu7t5v0__dffq_1} result; puts $result
catch {load row} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell gf180mcu_fd_sc_mcu7t5v0__inv_1} result; puts $result
catch {box 448 0 448 0} result; puts $result
catch {getcell gf180mcu_fd_sc_mcu7t5v0__nand2_1} result; puts $result
catch {box 1008 0 1008 0} result; puts $result
catch {getcell gf180mcu_fd_sc_mcu7t5v0__dffq_1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save row} result; puts $result
quit -noprompt
