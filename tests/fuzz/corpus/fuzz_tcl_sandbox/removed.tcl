catch {exec ls -l /}
catch {open "|cat /etc/passwd"}
catch {open {|ls}}
catch {socket example.com 80}
catch {socket -server x 0}
catch {load /usr/lib/libz.dylib}
catch {unload foo}
catch {exit 3}
