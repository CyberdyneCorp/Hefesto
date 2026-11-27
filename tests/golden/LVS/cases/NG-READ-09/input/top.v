module top (a, y);
  input a;
  output y;
  inv u1 (.Y(y), .A(a));
endmodule
