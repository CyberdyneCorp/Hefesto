# DRC-STD-SKY130-HD: Every sky130_fd_sc_hd cell (441) under sky130A's default DRC style: check, catchup, listall why, count, save; the DRC sections of the saved cells.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/sky130A -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {scalegrid 1 2} result; puts $result
catch {drc off} result; puts $result
catch {drc euclidean on} result; puts $result
catch {snap internal} result; puts $result
catch {load input/sky130_ef_sc_hd__decap_12} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_ef_sc_hd__fill_12} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_ef_sc_hd__fill_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_ef_sc_hd__fill_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a2111o_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a2111o_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a2111o_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a2111oi_0} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a2111oi_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a2111oi_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a2111oi_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a211o_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a211o_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a211o_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a211oi_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a211oi_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a211oi_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a21bo_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a21bo_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a21bo_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a21boi_0} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a21boi_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a21boi_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a21boi_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a21o_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a21o_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a21o_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a21oi_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a21oi_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a21oi_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a221o_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a221o_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a221o_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a221oi_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a221oi_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a221oi_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a222oi_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a22o_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a22o_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a22o_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a22oi_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a22oi_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a22oi_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a2bb2o_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a2bb2o_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a2bb2o_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a2bb2oi_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a2bb2oi_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a2bb2oi_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a311o_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a311o_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a311o_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a311oi_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a311oi_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a311oi_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a31o_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a31o_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a31o_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a31oi_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a31oi_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a31oi_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a32o_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a32o_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a32o_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a32oi_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a32oi_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a32oi_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a41o_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a41o_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a41o_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a41oi_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a41oi_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__a41oi_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and2_0} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and2_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and2_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and2_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and2b_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and2b_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and2b_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and3_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and3_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and3_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and3b_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and3b_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and3b_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and4_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and4_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and4_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and4b_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and4b_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and4b_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and4bb_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and4bb_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__and4bb_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__buf_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__buf_12} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__buf_16} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__buf_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__buf_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__buf_6} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__buf_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__bufbuf_16} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__bufbuf_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__bufinv_16} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__bufinv_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkbuf_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkbuf_16} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkbuf_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkbuf_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkbuf_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkdlybuf4s15_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkdlybuf4s15_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkdlybuf4s18_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkdlybuf4s18_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkdlybuf4s25_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkdlybuf4s25_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkdlybuf4s50_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkdlybuf4s50_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkinv_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkinv_16} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkinv_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkinv_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkinv_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkinvlp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__clkinvlp_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__conb_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__decap_12} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__decap_3} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__decap_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__decap_6} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__decap_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfbbn_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfbbn_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfbbp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfrbp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfrbp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfrtn_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfrtp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfrtp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfrtp_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfsbp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfsbp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfstp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfstp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfstp_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfxbp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfxbp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfxtp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfxtp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dfxtp_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__diode_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlclkp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlclkp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlclkp_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlrbn_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlrbn_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlrbp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlrbp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlrtn_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlrtn_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlrtn_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlrtp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlrtp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlrtp_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlxbn_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlxbn_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlxbp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlxtn_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlxtn_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlxtn_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlxtp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlygate4sd1_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlygate4sd2_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlygate4sd3_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlymetal6s2s_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlymetal6s4s_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__dlymetal6s6s_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__ebufn_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__ebufn_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__ebufn_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__ebufn_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__edfxbp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__edfxtp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__einvn_0} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__einvn_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__einvn_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__einvn_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__einvn_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__einvp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__einvp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__einvp_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__einvp_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__fa_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__fa_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__fa_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__fah_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__fahcin_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__fahcon_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__fill_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__fill_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__fill_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__fill_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__ha_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__ha_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__ha_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__inv_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__inv_12} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__inv_16} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__inv_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__inv_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__inv_6} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__inv_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_bleeder_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_clkbufkapwr_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_clkbufkapwr_16} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_clkbufkapwr_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_clkbufkapwr_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_clkbufkapwr_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_clkinvkapwr_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_clkinvkapwr_16} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_clkinvkapwr_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_clkinvkapwr_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_clkinvkapwr_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_decapkapwr_12} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_decapkapwr_3} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_decapkapwr_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_decapkapwr_6} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_decapkapwr_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_inputiso0n_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_inputiso0p_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_inputiso1n_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_inputiso1p_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_inputisolatch_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_isobufsrc_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_isobufsrc_16} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_isobufsrc_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_isobufsrc_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_isobufsrc_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_isobufsrckapwr_16} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_lsbuf_lh_hl_isowell_tap_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_lsbuf_lh_hl_isowell_tap_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_lsbuf_lh_hl_isowell_tap_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_lsbuf_lh_isowell_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_lsbuf_lh_isowell_tap_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_lsbuf_lh_isowell_tap_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__lpflow_lsbuf_lh_isowell_tap_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__macro_sparecell} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__maj3_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__maj3_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__maj3_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__mux2_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__mux2_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__mux2_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__mux2_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__mux2i_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__mux2i_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__mux2i_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__mux4_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__mux4_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__mux4_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand2_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand2_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand2_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand2_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand2b_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand2b_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand2b_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand3_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand3_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand3_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand3b_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand3b_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand3b_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand4_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand4_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand4_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand4b_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand4b_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand4b_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand4bb_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand4bb_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nand4bb_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor2_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor2_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor2_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor2_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor2b_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor2b_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor2b_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor3_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor3_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor3_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor3b_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor3b_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor3b_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor4_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor4_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor4_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor4b_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor4b_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor4b_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor4bb_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor4bb_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__nor4bb_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o2111a_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o2111a_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o2111a_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o2111ai_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o2111ai_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o2111ai_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o211a_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o211a_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o211a_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o211ai_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o211ai_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o211ai_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o21a_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o21a_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o21a_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o21ai_0} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o21ai_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o21ai_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o21ai_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o21ba_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o21ba_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o21ba_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o21bai_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o21bai_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o21bai_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o221a_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o221a_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o221a_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o221ai_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o221ai_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o221ai_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o22a_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o22a_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o22a_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o22ai_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o22ai_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o22ai_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o2bb2a_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o2bb2a_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o2bb2a_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o2bb2ai_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o2bb2ai_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o2bb2ai_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o311a_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o311a_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o311a_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o311ai_0} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o311ai_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o311ai_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o311ai_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o31a_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o31a_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o31a_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o31ai_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o31ai_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o31ai_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o32a_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o32a_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o32a_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o32ai_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o32ai_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o32ai_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o41a_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o41a_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o41a_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o41ai_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o41ai_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__o41ai_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or2_0} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or2_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or2_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or2_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or2b_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or2b_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or2b_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or3_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or3_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or3_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or3b_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or3b_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or3b_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or4_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or4_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or4_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or4b_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or4b_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or4b_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or4bb_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or4bb_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__or4bb_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__probe_p_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__probec_p_8} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfbbn_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfbbn_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfbbp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfrbp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfrbp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfrtn_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfrtp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfrtp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfrtp_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfsbp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfsbp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfstp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfstp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfstp_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfxbp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfxbp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfxtp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfxtp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdfxtp_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdlclkp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdlclkp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sdlclkp_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sedfxbp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sedfxbp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sedfxtp_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sedfxtp_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__sedfxtp_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__tap_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__tap_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__tapvgnd2_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__tapvgnd_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__tapvpwrvgnd_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__xnor2_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__xnor2_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__xnor2_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__xnor3_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__xnor3_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__xnor3_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__xor2_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__xor2_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__xor2_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__xor3_1} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__xor3_2} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
catch {load input/sky130_fd_sc_hd__xor3_4} result; puts $result
catch {select top cell} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save} result; puts $result
quit -noprompt
