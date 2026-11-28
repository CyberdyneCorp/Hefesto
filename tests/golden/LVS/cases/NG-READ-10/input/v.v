`define W 2
module sub (input [1:0] d, output q);
endmodule
module top (a, b, y);
  input [`W-1:0] a;
  input b;
  output y;
  wire [1:0] n;
  assign n[0] = b;
  sub u0 (.d({a[0], n[0]}), .q(y));
  genvar i;
  generate for (i = 0; i < 2; i = i + 1) begin : g
    sub u (.d(a), .q(n[1]));
  end endgenerate
  specify
    (a => y) = 1;
  endspecify
endmodule
