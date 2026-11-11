// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The sg13g2 MOS devices of ihp-sg13g2-fet.tcl (IHP-Open-PDK 5e6d592e):
// {type, mos_check device argument, mos_draw lvsname argument, _defaults text (after
// Tcl's backslash-newline substitution), the draw procedure's newdict}. Included by
// sg13g2/mos.cpp. Generated from the PDK file; regenerate rather than edit by hand.

    {"sg13_lv_nmos", "sg13_lv_nmos", "none",
     "w 0.6 l 0.13 m 1 ng 1 diffcov 100 polycov 100  guard 0 glc 1 grc 1 gtc 1 gbc 1 tbcov 100 rlcov 100  topc 1 botc 1 poverlap 0 doverlap 1 lmin 0.13 wmin 0.15  class mosfet compatible {sg13_lv_nmos sg13_hv_nmos}  full_metal 1 conn_gates 0 gate_ring 0 doports 1  viasrc 100 viadrn 100 viagate 100 ccols 1  viagb 0 viagr 0 viagl 0 viagt 0",
     {{"gate_type", "nfet"}, {"diff_type", "ndiff"}, {"diff_contact_type", "ndc"}, {"plus_diff_type", "psd"}, {"plus_contact_type", "psc"}, {"poly_type", "poly"}, {"poly_contact_type", "pc"}, {"sub_type", "psub"}}},
    {"sg13_lv_pmos", "sg13_lv_pmos", "none",
     "w 0.6 l 0.13 m 1 ng 1 diffcov 100 polycov 100  guard 0 glc 1 grc 1 gtc 1 gbc 1 tbcov 100 rlcov 100  topc 1 botc 1 poverlap 0 doverlap 1 lmin 0.13 wmin 0.15  class mosfet compatible {sg13_lv_pmos sg13_hv_pmos}  full_metal 1 conn_gates 0 gate_ring 0 ccols 1  viasrc 100 viadrn 100 viagate 100  viagb 0 viagr 0 viagl 0 viagt 0 doports 1",
     {{"gate_type", "pfet"}, {"diff_type", "pdiff"}, {"diff_contact_type", "pdc"}, {"plus_diff_type", "nsd"}, {"plus_contact_type", "nsc"}, {"poly_type", "poly"}, {"poly_contact_type", "pc"}, {"sub_type", "nwell"}, {"dev_sub_type", "nwell"}}},
    {"sg13_hv_pmos", "sg13_hv_pmos", "none",
     "w 1.0 l 0.4 m 1 ng 1 diffcov 100 polycov 100  guard 0 glc 1 grc 1 gtc 1 gbc 1 tbcov 100 rlcov 100  topc 1 botc 1 poverlap 0 doverlap 1 lmin 0.4 wmin 0.15  class mosfet compatible {sg13_lv_pmos sg13_hv_pmos}  full_metal 1 conn_gates 0 gate_ring 0 ccols 1  viasrc 100 viadrn 100 viagate 100  viagb 0 viagr 0 viagl 0 viagt 0 doports 1",
     {{"gate_type", "hvpfet"}, {"diff_type", "hvpdiff"}, {"diff_contact_type", "hvpdc"}, {"plus_diff_type", "hvnsd"}, {"plus_contact_type", "hvnsc"}, {"poly_type", "poly"}, {"poly_contact_type", "pc"}, {"sub_type", "nwell"}, {"dev_sub_type", "nwell"}, {"sub_surround", "0.62"}, {"guard_sub_surround", "0.62"}}},
    {"sg13_hv_nmos", "sg13_hv_nmos", "none",
     "w 1.0 l 0.45 m 1 ng 1 diffcov 100 polycov 100  guard 0 glc 1 grc 1 gtc 1 gbc 1 tbcov 100 rlcov 100  topc 1 botc 1 poverlap 0 doverlap 1 lmin 0.45 wmin 0.15  class mosfet compatible {sg13_lv_nmos sg13_hv_nmos}  full_metal 1 conn_gates 0 gate_ring 0 doports 1  viasrc 100 viadrn 100 viagate 100 ccols 1  viagb 0 viagr 0 viagl 0 viagt 0",
     {{"gate_type", "hvnfet"}, {"diff_type", "hvndiff"}, {"diff_contact_type", "hvndc"}, {"plus_diff_type", "hvpsd"}, {"plus_contact_type", "hvpsc"}, {"poly_type", "poly"}, {"poly_contact_type", "pc"}, {"sub_type", "psub"}}},
    {"sg13_lv_rfnmos", "sg13_lv_rfnmos", "rfnmos",
     "w 0.6 l 0.13 m 1 ng 1 diffcov 100 polycov 100  glc 1 grc 1 gtc 1 gbc 1 tbcov 100 rlcov 100  poverlap 0 doverlap 1 lmin 0.13 wmin 0.15  class mosfet compatible {sg13_lv_rfnmos sg13_hv_rfnmos}  full_metal 1 doports 1  viasrc 100 viadrn 100 viagate 100 ccols 1  viagb 0 viagr 0 viagl 0 viagt 0",
     {{"is_rf", "1"}, {"gate_type", "nfet"}, {"diff_type", "ndiff"}, {"diff_contact_type", "ndc"}, {"plus_diff_type", "psd"}, {"plus_contact_type", "psc"}, {"poly_type", "poly"}, {"poly_contact_type", "pc"}, {"sub_type", "psub"}}},
    {"sg13_lv_rfpmos", "sg13_lv_rfpmos", "rfpmos",
     "w 0.6 l 0.13 m 1 ng 1 diffcov 100 polycov 100  glc 1 grc 1 gtc 1 gbc 1 tbcov 100 rlcov 100  poverlap 0 doverlap 1 lmin 0.13 wmin 0.15  class mosfet compatible {sg13_lv_rfpmos sg13_hv_rfpmos}  full_metal 1 ccols 1  viasrc 100 viadrn 100 viagate 100  viagb 0 viagr 0 viagl 0 viagt 0 doports 1",
     {{"is_rf", "1"}, {"gate_type", "pfet"}, {"diff_type", "pdiff"}, {"diff_contact_type", "pdc"}, {"plus_diff_type", "nsd"}, {"plus_contact_type", "nsc"}, {"poly_type", "poly"}, {"poly_contact_type", "pc"}, {"sub_type", "nwell"}, {"dev_sub_type", "nwell"}}},
    {"sg13_hv_rfnmos", "sg13_hv_rfnmos", "rfnmosHV",
     "w 1.0 l 0.45 m 1 ng 1 diffcov 100 polycov 100  glc 1 grc 1 gtc 1 gbc 1 tbcov 100 rlcov 100  poverlap 0 doverlap 1 lmin 0.45 wmin 0.15  class mosfet compatible {sg13_lv_rfnmos sg13_hv_rfnmos}  full_metal 1 doports 1  viasrc 100 viadrn 100 viagate 100 ccols 1  viagb 0 viagr 0 viagl 0 viagt 0",
     {{"is_rf", "1"}, {"gate_type", "hvnfet"}, {"diff_type", "hvndiff"}, {"diff_contact_type", "hvndc"}, {"plus_diff_type", "hvpsd"}, {"plus_contact_type", "hvpsc"}, {"poly_type", "poly"}, {"poly_contact_type", "pc"}, {"sub_type", "psub"}}},
    {"sg13_hv_rfpmos", "sg13_hv_rfpmos", "rfpmosHV",
     "w 1.0 l 0.4 m 1 ng 1 diffcov 100 polycov 100  glc 1 grc 1 gtc 1 gbc 1 tbcov 100 rlcov 100  poverlap 0 doverlap 1 lmin 0.4 wmin 0.15  class mosfet compatible {sg13_lv_rfpmos sg13_hv_rfpmos}  full_metal 1 ccols 1  viasrc 100 viadrn 100 viagate 100  viagb 0 viagr 0 viagl 0 viagt 0 doports 1",
     {{"is_rf", "1"}, {"gate_type", "hvpfet"}, {"diff_type", "hvpdiff"}, {"diff_contact_type", "hvpdc"}, {"plus_diff_type", "hvnsd"}, {"plus_contact_type", "hvnsc"}, {"poly_type", "poly"}, {"poly_contact_type", "pc"}, {"sub_type", "nwell"}, {"dev_sub_type", "nwell"}, {"guard_sub_surround", "0.62"}}},
