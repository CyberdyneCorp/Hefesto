file mkdir out
if 1 {
schematic open input/old.sym
schematic save out/old.sym
schematic close -discard
schematic open out/old.sym
schematic save out/2-old.sym
schematic close -discard
list
}
