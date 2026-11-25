.subckt ip a b
.ends
R1 a b 1k
C1 a b 1p
.model nch nmos
M1 d g s b nch
