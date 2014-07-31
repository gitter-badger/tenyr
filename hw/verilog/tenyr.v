`include "common.vh"
`timescale 1ns/10ps

module Reg(input clk, upZ,               input[ 3:0] indexZ, indexX, indexY,
           input[31:0] writeZ, next_pc, output[31:0] valueZ, valueX, valueY);

    reg[31:0] store[0:14];
    initial store[0] = 0;

    //              reg is P ?           reg is B-O
    assign valueX = &indexX ? next_pc : store[indexX];
    assign valueY = &indexY ? next_pc : store[indexY];
    assign valueZ = &indexZ ? next_pc : store[indexZ];

    always @(posedge clk) if (&{upZ,|indexZ,~&indexZ}) store[indexZ] <= writeZ;

endmodule

module Decode(input[31:0] insn, output[3:0] Z, X, Y, output[11:0] I,
              output[3:0] op, output[1:0] kind,
              output throw, storing, deref_rhs, branch, output[4:0] vector);

    assign {kind, storing, deref_rhs, Z, X, Y, op, I} = insn;
    assign vector = insn[4:0];
    assign throw  = &kind;
    assign branch = &Z & ~storing;

endmodule

module Shuf(input clk, input[1:0] kind, input[31:0] X, Y, input[11:0] I,
                                   output reg[31:0] A, B, C);

    wire[31:0] J = { {20{I[11]}}, I[11:0] };
    always @(posedge clk)
        case (kind)
            2'b00   : begin A <= X   ; B <= Y   ; C <= J   ; end
            2'b01   : begin A <= X   ; B <= J   ; C <= Y   ; end
            2'b10   : begin A <= J   ; B <= X   ; C <= Y   ; end
            default : begin A <= 'bx ; B <= 'bx ; C <= 'bx ; end
        endcase

endmodule

module Exec(input clk, en, output reg[31:0] rhs, input[3:0] op,
            input signed[31:0] A, B, C);

    always @(posedge clk) if (en)
        case (op)
            4'b0000: rhs <=  (A  |  B) + C; // X bitwise or Y
            4'b0001: rhs <=  (A  &  B) + C; // X bitwise and Y
            4'b0010: rhs <=  (A  +  B) + C; // X add Y
            4'b0011: rhs <=  (A  *  B) + C; // X multiply Y
            4'b0100: rhs <= 32'bx;          // reserved
            4'b0101: rhs <=  (A  << B) + C; // X shift left Y
            4'b0110: rhs <= -(A  <  B) + C; // X compare < Y
            4'b0111: rhs <= -(A  == B) + C; // X compare == Y
            4'b1000: rhs <= -(A  >= B) + C; // X compare >= Y
            4'b1001: rhs <=  (A  &~ B) + C; // X bitwise and complement Y
            4'b1010: rhs <=  (A  ^  B) + C; // X bitwise xor Y
            4'b1011: rhs <=  (A  -  B) + C; // X subtract Y
            4'b1100: rhs <=  (A  ^~ B) + C; // X xor ones' complement Y
            4'b1101: rhs <=  (A  >> B) + C; // X shift right logical Y
            4'b1110: rhs <= -(A  != B) + C; // X compare <> Y
            4'b1111: rhs <=  (A >>> B) + C; // X shift right arithmetic Y
        endcase

endmodule

module Core(input clk, reset_n, trap, inout wor `HALTTYPE halt, output strobe,
            output reg[31:0] i_addr, input[31:0] i_data, output mem_rw,
            output    [31:0] d_addr, input[31:0] d_in  , output[31:0] d_out);

    localparam[3:0] sI=0, s0=1, s1=2, s2=3, s3=4, s4=5, s5=6, s6=7, s7=8,
                    sE=8, sF=9, sR=10, sT=11, sW=12;

    wire throw, kind, drhs, jumping, storing, loading, deref;
    wire[ 3:0] indexX, indexY, indexZ, op;
    wire[31:0] valueX, valueY, valueZ, irhs, rhs, tostore;
    wire[31:0] valueA, valueB, valueC;
    wire[11:0] valueI;
    wire[ 4:0] vector;

    reg [31:0] r_irhs, r_data, next_pc, v_addr, insn;
    reg [3:0] state = sI;

    always @(posedge clk)
        if (!reset_n)
            state <= sI;
        else case (state)
            s0: begin state <= halt ? sI : trap ? sF : throw ? sE : s1; end
            s1: begin state <= s2; /* shuffle */                        end
            s2: begin state <= s3; r_irhs  <= irhs;                     end
            s3: begin state <= s4; /* compensate for slow multiplier */ end
            s4: begin state <= s5; r_data  <= d_in;                     end
            s5: begin state <= s6; i_addr  <= jumping ? rhs : next_pc;  end
            s6: begin state <= s7; next_pc <= i_addr + 1;               end
            s7: begin state <= s0; insn    <= i_data; /* why extra ? */ end
            // TODO return to instruction OF a trap, but AFTER a throw
            sE: begin state <= sR; r_irhs  <= `VECTOR_ADDR | vector;    end
            sR: begin state <= sT; v_addr  <= d_in;   /* test this */   end
            sF: begin state <= sT; v_addr  <= `TRAMP_BOTTOM;            end
            sT: begin state <= sW; r_irhs  <= i_addr; /* wait for */    end
            sW: begin state <= s6; i_addr  <= v_addr; /* trap's fall */ end
            sI: begin state <= halt ? sI : s6; i_addr <= `RESETVECTOR;  end
        endcase

    // Instruction fetch happens on cycle 0

    // Decode and register reads happen as soon as instruction is ready
    Decode decode(.Z ( indexZ ), .insn  ( insn   ), .storing   ( storing ),
                  .X ( indexX ), .kind  ( kind   ), .deref_rhs ( drhs    ),
                  .Y ( indexY ), .op    ( op     ), .branch    ( jumping ),
                  .I ( valueI ), .throw ( throw  ), .vector    ( vector  ));

    // Shuffle occurs on cycle 0
    Shuf shuf(.clk ( clk  ), .A( valueA ), .B( valueB ), .C( valueC ),
              .kind( kind ), .X( valueX ), .Y( valueY ), .I( valueI ));

    // Execution (arithmetic operation) occurs on cycle 1
    wire en_ex = state == s1;
    Exec exec(.clk ( clk    ), .en ( en_ex  ), .op ( op     ),
              .A   ( valueA ), .B  ( valueB ), .C  ( valueC ), .rhs ( irhs ));

    // Memory loads or stores on cycle 3
    assign loading = (drhs && !mem_rw) || state == sR;
    assign strobe  = (state == s4 && (loading || storing)) || state == sW;
    assign mem_rw  = storing || state == sW;
    assign tostore = state == sW ? `TRAP_ADDR : valueZ;
    assign deref   = drhs && (state != sT);
    assign rhs     = deref ? r_data : r_irhs;
    assign d_out   = deref ? valueZ : r_irhs;
    assign d_addr  = deref ? r_irhs : tostore;

    // Registers commit on cycle 4
    wire updateZ = !storing && state == s5;
    Reg regs(.clk     ( clk     ), .indexX ( indexX ), .valueX ( valueX  ),
             .next_pc ( next_pc ), .indexY ( indexY ), .valueY ( valueY  ),
                                   .indexZ ( indexZ ), .valueZ ( valueZ  ),
                                   .writeZ ( rhs    ), .upZ    ( updateZ ));

endmodule

