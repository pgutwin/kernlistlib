// Test 1: Simple x value
module test1;
  wire [1:0] a;
  assign a = 2'hx;
endmodule

// Test 2: Simple concatenation on right side
module test2;
  wire [3:0] a, b, c;
  assign a = {b, c};
endmodule

// Test 3: Simple concatenation on left side (LHS concat)
//   BIG NOTE - my original BNF specifically mentioned that this was not
//   going to be supported. So I'm going to deprecate this test for now.
// module test3;
//   wire [7:0] data;
//   assign {data[7:4], data[3:0]} = 8'h0;
// endmodule

// Test 4: Number with x in concat
module test4;
  wire [3:0] a;
  assign a = {2'h0, 2'hx};
endmodule
