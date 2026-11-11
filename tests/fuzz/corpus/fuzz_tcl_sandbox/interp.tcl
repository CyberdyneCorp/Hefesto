catch {interp create unsafe}
interp create -safe c
catch {c invokehidden exec ls}
catch {interp invokehidden c exec ls}
catch {interp expose c open}
catch {interp marktrusted c}
c eval {interp create gc}
catch {interp invokehidden {c gc} source /etc/passwd}
catch {c eval {gc invokehidden exec ls}}
interp delete c
catch {package require Tk}
package ifneeded evil 1.0 {load ./libevil.so}
catch {package require evil}
package require msgcat
msgcat::mc hello
clock format 0 -gmt 1
catch {encoding dirs /etc}
catch {encoding convertfrom nosuch abc}
