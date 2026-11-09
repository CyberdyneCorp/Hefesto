# UI-G-032: windowcaption; windowcaption off; openwindow; windowborder; windowscrollbars on.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {windowcaption} result; puts $result
catch {windowcaption off} result; puts $result
catch {openwindow} result; puts $result
catch {windowcaption} result; puts $result
catch {windowborder} result; puts $result
catch {windowscrollbars on} result; puts $result
catch {windowcaption on} result; puts $result
quit -noprompt
