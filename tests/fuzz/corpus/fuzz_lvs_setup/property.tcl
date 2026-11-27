property {nfet 0} add {nf integer 0} {w2 double 0.02} {m string 3} {e expression 2*w} x 1
property {nfet 0} tolerance {W inf} {L 0.05} bad
property {nfet 0} associate {W drain}
property {nfet 0} series {W add} {L par} enable
property {nfet 0} parallel {W critical} off
property {nfet 0} merge {W par_critical} {L ser}
property parallel none
property parallel open
property series all
property topology strict
property {rpoly 1} derive area w l
property {nfet 0} remove
