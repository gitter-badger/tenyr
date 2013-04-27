`include "common.vh"
`timescale 1ns/10ps

module lookup7(clk, char, out);

    input clk;
    input[6:0] char;
    output reg[7:0] out = 0;

    always @(posedge clk) begin
        case (char)
           7'd032 /* ' ' */: out = 8'b11111111;
           7'd045 /* '-' */: out = 8'b10111111;

           7'd048 /* '0' */: out = 8'b11000000;
           7'd049 /* '1' */: out = 8'b11111001;
           7'd050 /* '2' */: out = 8'b10100100;
           7'd051 /* '3' */: out = 8'b10110000;
           7'd052 /* '4' */: out = 8'b10011001;
           7'd053 /* '5' */: out = 8'b10010010;
           7'd054 /* '6' */: out = 8'b10000010;
           7'd055 /* '7' */: out = 8'b11111000;
           7'd056 /* '8' */: out = 8'b10000000;
           7'd057 /* '9' */: out = 8'b10010000;

           7'd065 /* 'A' */: out = 8'b10001000;
           7'd067 /* 'C' */: out = 8'b11000110;
           7'd069 /* 'E' */: out = 8'b10000110;
           7'd070 /* 'F' */: out = 8'b10001110;
           7'd071 /* 'G' */: out = 8'b10000010;
           7'd072 /* 'H' */: out = 8'b10001001;
           7'd073 /* 'I' */: out = 8'b11111001;
           7'd074 /* 'J' */: out = 8'b11110001;
           7'd076 /* 'L' */: out = 8'b11000111;
           7'd079 /* 'O' */: out = 8'b11000000;
           7'd080 /* 'P' */: out = 8'b10001100;
           7'd083 /* 'S' */: out = 8'b10010010;
           7'd085 /* 'U' */: out = 8'b11000001;
           7'd089 /* 'Y' */: out = 8'b10010001;
           7'd090 /* 'Z' */: out = 8'b10100100;

           7'd098 /* 'b' */: out = 8'b10000011;
           7'd099 /* 'c' */: out = 8'b10100111;
           7'd100 /* 'd' */: out = 8'b10100001;
           7'd102 /* 'f' */: out = 8'b10001110;
           7'd103 /* 'g' */: out = 8'b10010000;
           7'd104 /* 'h' */: out = 8'b10001011;
           7'd105 /* 'i' */: out = 8'b11111001; // TODO change to one-segment version
           7'd106 /* 'j' */: out = 8'b11110001;
           7'd111 /* 'o' */: out = 8'b10100011;
           7'd114 /* 'r' */: out = 8'b11100111;
           //7'd116 /* 't' */: out = 8'b11000111; // TODO
           7'd117 /* 'u' */: out = 8'b11100011;
           7'd121 /* 'y' */: out = 8'b10010001;
           7'd122 /* 'z' */: out = 8'b10100100;

           default         : out = 8'b0xxxxxxx; // indicate bad digit with decimal point
        endcase
    end

endmodule

