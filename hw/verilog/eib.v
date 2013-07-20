`include "common.vh"
`timescale 1ns/10ps

// External Interrupt Block
// contains its own RAMs and presents a simple interrupt handler interface
// to the tenyr Core
module Eib(input clk, reset_n, strobe, rw,
           input[IRQ_COUNT-1:0] irq, output reg trap,
           input[31:0] i_addr, output [31:0] i_data,
           input[31:0] d_addr, inout  [31:0] d_data);

    localparam IRQ_COUNT    = 32;               // total count of interrupts
    localparam DEPTH_BITS   = 5;                // maximum depth of stacks
    localparam MAX_DEPTH    = 1 << DEPTH_BITS;  // depth of stacks in words

    parameter  VECTS_BOTTOM = `VECTOR_ADDR;
    localparam VECTS_BITS   = 5;                // vector table size in bits
    localparam VECTS_SIZE   = 1 << VECTS_BITS;  // vector table size in words
    parameter  VECTORFILE   = "vectors.memh";

    parameter  STACK_BOTTOM = `ISTACK_BOTTOM;
    parameter  STACK_BITS   = 5;                // interrupt stack size in bits
    localparam STACK_SIZE   = 1 << STACK_BITS;  // interrupt stack size in words
    localparam STACK_WORDS  = (MAX_DEPTH << STACK_BITS) - 1;

    parameter  TRAMP_BOTTOM = `TRAMP_BOTTOM;
    parameter  TRAMP_BITS   = 8;                // trampoline size in bits
    localparam TRAMP_SIZE   = 1 << TRAMP_BITS;  // trampoline size in words

    reg [31:0] d_rdata, i_rdata;                // output on bus data lines

    reg [DEPTH_BITS-1:0] depth = 0;             // stack pointer
    reg [ IRQ_COUNT-1:0] isr = 0;               // Interrupt Status Register

    wire d_inrange = d_addr[31:12] == 20'hfffff;
    wire i_inrange = i_addr[31:12] == 20'hfffff;
    wire d_active  = d_inrange && strobe;
    assign d_data  = (d_active & ~rw) ? d_rdata : 32'bz;
    assign i_data  = i_inrange ? i_rdata : 32'bz;

`define IS_STACK(X)     (X[31:STACK_BITS] == STACK_BOTTOM[31:STACK_BITS])
`define IS_TRAMP(X)     (X[31:TRAMP_BITS] == TRAMP_BOTTOM[31:TRAMP_BITS])
`define IS_VECTS(X)     (X[31:VECTS_BITS] == VECTS_BOTTOM[31:VECTS_BITS])
`define STACK_ADDR(X)   ((depth << STACK_BITS) | X[(STACK_BITS - 1):0])
`define TRAMP_ADDR(X)   (X[(TRAMP_BITS - 1):0])
`define VECTS_ADDR(X)   (X[(VECTS_BITS - 1):0])

    wire[31:0] vects_dout_d, vects_dout_i;
    wire[31:0] stack_dout_d, stack_dout_i;
    wire[31:0] tramp_dout_d, tramp_dout_i;
    reg [31:0] cntrl_dout_d, cntrl_dout_i; // XXX cntrl_dout_i is never set

    wire[31:0] vects_ad = `VECTS_ADDR(d_addr), vects_ai = `VECTS_ADDR(i_addr);
    wire[31:0] stack_ad = `STACK_ADDR(d_addr), stack_ai = `STACK_ADDR(i_addr);
    wire[31:0] tramp_ad = `TRAMP_ADDR(d_addr), tramp_ai = `TRAMP_ADDR(i_addr);

    wire d_is_vects = d_active  && `IS_VECTS(d_addr),
         i_is_vects = i_inrange && `IS_VECTS(i_addr),
         d_is_tramp = d_active  && `IS_TRAMP(d_addr),
         i_is_tramp = i_inrange && `IS_TRAMP(i_addr),
         d_is_stack = d_active  && `IS_STACK(d_addr),
         i_is_stack = i_inrange && `IS_STACK(i_addr);
     
    wire d_is_cntrl = !(d_is_vects | d_is_tramp | d_is_stack);
    wire i_is_cntrl = !(i_is_vects | i_is_tramp | i_is_stack);

    ramwrap #( // TODO vects doesn't need to be accessible to instruction port
        .LOAD(1), .LOADFILE(VECTORFILE), .ABITS(VECTS_BITS), .SIZE(VECTS_SIZE),
        .BASE_A(VECTS_BOTTOM)
    ) vects(
        .clka  ( clk          ), .clkb  ( clk          ),
        .ena   ( d_active     ), .enb   ( i_inrange    ),
        .wea   ( rw           ), .web   ( 1'b0         ),
        .addra ( vects_ad     ), .addrb ( vects_ai     ),
        .dina  ( d_data       ), .dinb  ( 32'bx        ),
        .douta ( vects_dout_d ), .doutb ( vects_dout_i )
    );

    ramwrap #(
        .LOAD(1), .LOADFILE("../verilog/trampoline.memh"),
        .ABITS(TRAMP_BITS), .SIZE(TRAMP_SIZE), .BASE_A(TRAMP_BOTTOM)
    ) tramp(
        .clka  ( clk          ), .clkb  ( clk          ),
        .ena   ( d_active     ), .enb   ( i_inrange    ),
        .wea   ( rw           ), .web   ( 1'b0         ),
        .addra ( tramp_ad     ), .addrb ( tramp_ai     ),
        .dina  ( d_data       ), .dinb  ( 32'bx        ),
        .douta ( tramp_dout_d ), .doutb ( tramp_dout_i )
    );

    ramwrap #(
        .ABITS(STACK_BITS), .SIZE(STACK_SIZE), .BASE_A(STACK_BOTTOM)
    ) stack(
        .clka  ( clk          ), .clkb  ( clk          ),
        .ena   ( d_active     ), .enb   ( i_inrange    ),
        .wea   ( rw           ), .web   ( 1'b0         ),
        .addra ( stack_ad     ), .addrb ( stack_ai     ),
        .dina  ( d_data       ), .dinb  ( 32'bx        ),
        .douta ( stack_dout_d ), .doutb ( stack_dout_i )
    );

    wire       ra_active = d_addr[11:0] == 12'hfff;
    wire       im_active = d_addr[11:0] == 12'hffd;
    wire       pushing   = ra_active && d_active && rw;
    wire       imrs_rw   = im_active || pushing;
    wire[31:0] imrs_addr = depth + pushing;
    wire[31:0] imrs_din  = pushing ? 32'b0 : d_data;
    wire[31:0] imrs_dout;

    BlockRAM #(.ABITS(DEPTH_BITS), .SIZE(MAX_DEPTH), .INIT(1)) imrs(
        .clka ( clk      ), .addra ( imrs_addr ), .clkb ( 0 ), .addrb ( 0 ),
        .ena  ( d_active ), .dina  ( imrs_din  ), .enb  ( 0 ), .dinb  ( 0 ),
        .wea  ( imrs_rw  ), .douta ( imrs_dout ), .web  ( 0 )
    );

    wire[31:0] rets_addr = ra_active ? depth + 1 : depth;
    wire[31:0] rets_din  = d_data;
    wire[31:0] rets_dout;

    BlockRAM #(.ABITS(DEPTH_BITS), .SIZE(MAX_DEPTH), .INIT(1)) rets(
        .clka ( clk       ), .addra ( rets_addr ), .clkb ( 0 ), .addrb ( 0 ),
        .ena  ( d_active  ), .dina  ( rets_din  ), .enb  ( 0 ), .dinb  ( 0 ),
        .wea  ( ra_active ), .douta ( rets_dout ), .web  ( 0 )
    );

    always @*
        case ({1'b0,i_is_stack,i_is_tramp,i_is_vects})
            4'b0100: i_rdata = stack_dout_i;
            4'b0010: i_rdata = tramp_dout_i;
            4'b0001: i_rdata = vects_dout_i;
            default: i_rdata = 32'bx;
        endcase

    always @*
        case ({d_is_cntrl,d_is_stack,d_is_tramp,d_is_vects})
            4'b1000: d_rdata = cntrl_dout_d;
            4'b0100: d_rdata = stack_dout_d;
            4'b0010: d_rdata = tramp_dout_d;
            4'b0001: d_rdata = vects_dout_d;
            default: d_rdata = 32'bx;
        endcase

    always @(posedge clk)
        if (reset_n) case (d_addr[11:0])
            12'hfff: cntrl_dout_d <= rets_dout;  // RA  read
            12'hffe: cntrl_dout_d <= isr;        // ISR read
            12'hffd: cntrl_dout_d <= imrs_dout;  // IMR read
            default: cntrl_dout_d <= 32'bx;
        endcase

    always @(posedge clk)
        if (!reset_n) begin
            isr   <= 0;
            depth <= 0;
        end else begin
            isr  <= isr | irq;  // accumulate until cleared
            // For now, trap follows irq by one cycle
            trap <= |(imrs_dout & isr);

            if (d_active && d_is_cntrl) begin
                if (rw) begin   // writing
                    if (ra_active)
                        depth <= depth + 1;
                    if (im_active)
                        isr <= (isr | irq) & ~d_data;  // ISR bit clear
                end else
                    if (ra_active)
                        depth <= depth - 1;
            end
        end

    always @(posedge clk)   // Simulation self-checking
        if (reset_n && d_active && d_is_cntrl) begin
            if (ra_active) begin
                if (rw && depth == MAX_DEPTH - 1) begin
                    $display("Tried to push too many interrupt frames");
                    $stop;
                end else if (!rw && depth == 0) begin
                    $display("Tried to pop interrupt when empty");
                    $stop;
                end
            end else case (d_addr[11:0])
                12'hfff: ;
                12'hffe: ;
                12'hffd: ;
                default:
                    if (rw)
                        $display("Unhandled write of %x @ %x", d_data, d_addr);
                    else
                        $display("Unhandled read @ %x", d_addr);
            endcase
        end

endmodule

