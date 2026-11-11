rename open realopen
rename ::tcl::file::exists x
catch {x /etc/passwd}
catch {realopen |ls}
interp alias {} o {} realopen
catch {o /etc/passwd}
