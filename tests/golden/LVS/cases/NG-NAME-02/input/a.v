module leaf (p, q);
  input p;
  output q;
endmodule
module top (Vdd, \a[1] );
  input Vdd;
  input \a[1] ;
  leaf u1 (.p(Vdd), .q(\a[1] ));
endmodule
