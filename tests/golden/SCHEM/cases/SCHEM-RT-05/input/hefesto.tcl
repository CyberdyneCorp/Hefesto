file mkdir out
if 1 {
schematic open input/embed.sch
schematic save out/embed.sch
schematic close -discard
schematic open out/embed.sch
schematic save out/2-embed.sch
schematic close -discard
list
}
