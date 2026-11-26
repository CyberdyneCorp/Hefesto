file mkdir out
if 1 {
# SCHEM-PR-01: xschem get_tok / subst_tok on property strings. One line per call: the call, then the
# result between brackets.
proc show {args} {
  if {[catch {set r [uplevel #0 $args]} err]} { set r "error: $err" }
  puts "[list {*}$args] -> \[$r\]"
}
set strings [list \
  {name=C4 m=1 device="tantalium capacitor"} \
  {a = 1;b=2} \
  {a=1;b=2} \
  {k="x y"} \
  {k=\"q\"} \
  {k} \
  {} \
  {name=p1} \
  {name=x1 value=1} \
  "type=nmos\nformat=\"@name\"" \
  "drc=\"f {@name\\}\""]
foreach s $strings {
  foreach key {name m device a b k type format drc value lab} {
    show xschem get_tok $s $key
    show xschem get_tok $s $key 1
    show xschem get_tok $s $key 2
  }
}
show xschem subst_tok {name=p1} lab VDD
show xschem subst_tok {name=p1 } lab VDD
show xschem subst_tok {} lab VDD
show xschem subst_tok {name=x1 value=1} value {.lib a b}
show xschem subst_tok {name=x1 value=1} name {a b}
show xschem subst_tok {name=x1 value=1 m=2} value {}
show xschem subst_tok {k name=x1} k 5
show xschem subst_tok {a=1;b=2} b 3
show xschem subst_tok "name=x1\nvalue=1" value 2
list
}
