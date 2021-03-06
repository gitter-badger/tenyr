`include "common.vh"
`timescale 1ns/10ps

module BlockRAM(
    input clka, ena, wea, clkb, enb, web, output reg acka, ackb,
    input[PBITS-1:0] addra, input[DBITS-1:0] dina, output reg[DBITS-1:0] douta,
    input[PBITS-1:0] addrb, input[DBITS-1:0] dinb, output reg[DBITS-1:0] doutb
);

    parameter INIT     = 0;
    parameter ZERO     = 0;
    parameter LOAD     = 0;
    parameter LOADFILE = "default.memh";
    parameter PBITS    = 32; // port address bits
    parameter ABITS    = 10; // internal address bits
    parameter DBITS    = 32;
    parameter SIZE     = 1 << ABITS;
    parameter OFFSET   = 0;

    reg[DBITS-1:0] store[SIZE - 1:0];

    integer i;
    initial begin
        if (INIT)
            for (i = 0; i < SIZE; i = i + 1)
                store[i] = ZERO;
        if (LOAD) $readmemh(LOADFILE, store);
    end

    always @(posedge clka) begin
        if (wea && ena)
            store[addra - OFFSET] <= dina;
        douta <= store[addra - OFFSET];
        acka <= ena;
    end

    always @(posedge clkb) begin
        if (web && enb)
            store[addrb - OFFSET] <= dinb;
        doutb <= store[addrb - OFFSET];
        ackb <= enb;
    end

endmodule

