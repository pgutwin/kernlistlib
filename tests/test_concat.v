// This should fail according to the note in my BNF. Don't use this test.
module test;
  wire [31:0] coef;
  assign { coef[31:29], coef[27:24], coef[16], coef[12:11], coef[9:8], coef[5:4], coef[2] } = { coef[19], coef[19], coef[15], coef[13], coef[15], coef[19], coef[14], coef[19], coef[19:18], coef[20], coef[21], 2'hx, coef[3] };
endmodule
