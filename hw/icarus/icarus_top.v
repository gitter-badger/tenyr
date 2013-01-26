`include "common.vh"
`timescale 1ns/10ps

module Top();
    Tenyr tenyr();

    reg [100:0] filename;
    initial #0 begin
        $dumpfile("Top.vcd");
        if ($value$plusargs("LOAD=%s", filename))
            $tenyr_load(filename);
        $dumpvars;
        #(64 * `CLOCKPERIOD) $finish;
    end
endmodule

module Tenyr(output[7:0] seg, output[3:0] an);
    reg reset_n = 0;
    reg rhalt = 1;

    reg  _clk_core = 1;
    reg  en_core   = 0;
    wire clk_core  = en_core & _clk_core;

    // TODO proper data clock timing
    wire clk_datamem = ~clk_core;
    wire clk_insnmem = clk_core;

    always #(`CLOCKPERIOD / 2) begin
        _clk_core = ~_clk_core;
        en_core = ~rhalt;
    end

    wire[31:0] insn_addr, operand_addr, insn_data, out_data;
    wire[31:0] in_data      =  operand_rw ? operand_data : 32'bx;
    wire[31:0] operand_data = !operand_rw ?     out_data : 32'bz;

    wire operand_rw;

    // rhalt and reset_n timing should be independent of each other, and
    // do indeed appear to be so.
    initial #(4 * `CLOCKPERIOD) rhalt = 0;
    initial #(3 * `CLOCKPERIOD) reset_n = 1;

`ifdef DEMONSTRATE_HALT
    initial #(21 * `CLOCKPERIOD) rhalt = 1;
    initial #(23 * `CLOCKPERIOD) rhalt = 0;
`endif

    wire[`HALTBUSWIDTH-1:0] halt;
    assign halt[`HALT_SIM] = rhalt;
    assign halt[`HALT_TENYR] = rhalt;

    // active on posedge clock
    SimMem #(.BASE(`RESETVECTOR))
        ram(.clka(~clk_datamem), .wea(operand_rw), .addra(operand_addr),
            .dina(in_data), .douta(out_data),
            .clkb(~clk_insnmem), .web(1'b0), .addrb(insn_addr),
            .dinb(32'bx), .doutb(insn_data));

    SimSerial serial(.clk(clk_datamem), .reset_n(reset_n), .enable(!halt),
                     .rw(operand_rw), .addr(operand_addr),
                     .data(operand_data));

    Seg7 #(.BASE(12'h100))
             seg7(.clk(clk_datamem), .reset_n(reset_n), .enable(1'b1), // XXX use halt ?
                  .rw(operand_rw), .addr(operand_addr),
                  .data(operand_data), .seg(seg), .an(an));

    // TODO clkL
    Core core(.clk(clk_core),
              .en(1'b1), .reset_n(reset_n), .mem_rw(operand_rw),
              .d_addr(operand_addr), .d_data(operand_data),
              .i_addr(insn_addr)   , .i_data(insn_data), .halt(halt));
endmodule

