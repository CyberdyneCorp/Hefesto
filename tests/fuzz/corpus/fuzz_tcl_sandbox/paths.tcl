catch {open /etc/passwd}
catch {open ../../../../etc/passwd}
catch {source /etc/profile}
catch {file delete /tmp/x}
catch {file mkdir /tmp/hefesto-fuzz-escape}
catch {file copy x /tmp/y}
catch {file rename x ../y}
catch {file exists ~/.ssh}
catch {glob -nocomplain /etc/*}
catch {glob -directory .. *}
catch {file link -symbolic out /etc}
catch {file tempfile n}
catch {file tempfile n /tmp/xXXXX}
catch {open //etc/hosts}
catch {file stat /etc st}
catch {cd /}
catch {cd ~}
catch {cd ..}
