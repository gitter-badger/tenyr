`include "common.vh"
`timescale 1ns/10ps

// basic 7-segment driver
module Seg7(input clk, strobe, rw, reset, input[31:0] addr, d_in,
            output reg[31:0] d_out, output[7:0] seg, output[DIGITS-1:0] an);

    parameter       CNT_BITS = 16 + 2; // 80MHz clk => 2ms full screen refresh
    localparam      DIGITS   = 4;

    reg[CNT_BITS-1:0] counter = 0;
    reg[DIGITS*4-1:0] store;
    reg[3:0] dots;

    wire[1:0] dig = counter[CNT_BITS - 2 +: 2];
    assign seg[7] = (dots[0 +: 4] & ~an) == 0;
    assign an = ~(1 << dig);
    Hex2Segments lookup(clk, store[dig * 4 +: 4], seg[6:0]);

    always @(posedge clk) begin
        if (reset) begin
            counter <= 0;
            store   <= 0;
            dots    <= 0;
        end else begin
            counter <= counter + 1;
            if (strobe) begin
                if (rw) begin
                    case (addr[0])
                        1'b0: store <= d_in;
                        1'b1: dots  <= d_in;
                    endcase
                end
            end
        end
    end

    always @* begin
        case (addr[0])
            1'b0: d_out <= store;
            1'b1: d_out <= dots;
        endcase
    end

endmodule

