`default_nettype wire

module font_rom (
    input logic [3:0] digit,
    input logic [2:0] row, // 0 to 4
    output logic [4:0] bitmap
);

    always_comb begin
        case (digit)
            4'd0: case(row)
                    3'd0: bitmap = 5'b01110;
                    3'd1: bitmap = 5'b10001;
                    3'd2: bitmap = 5'b10001;
                    3'd3: bitmap = 5'b10001;
                    3'd4: bitmap = 5'b01110;
                    default: bitmap = 0;
                  endcase
            4'd1: case(row)
                    3'd0: bitmap = 5'b00100;
                    3'd1: bitmap = 5'b01100;
                    3'd2: bitmap = 5'b00100;
                    3'd3: bitmap = 5'b00100;
                    3'd4: bitmap = 5'b01110;
                    default: bitmap = 0;
                  endcase
            4'd2: case(row)
                    3'd0: bitmap = 5'b01110;
                    3'd1: bitmap = 5'b10001;
                    3'd2: bitmap = 5'b00010;
                    3'd3: bitmap = 5'b00100;
                    3'd4: bitmap = 5'b11111;
                    default: bitmap = 0;
                  endcase
            4'd3: case(row)
                    3'd0: bitmap = 5'b11110;
                    3'd1: bitmap = 5'b00001;
                    3'd2: bitmap = 5'b01110;
                    3'd3: bitmap = 5'b00001;
                    3'd4: bitmap = 5'b11110;
                    default: bitmap = 0;
                  endcase
            4'd4: case(row)
                    3'd0: bitmap = 5'b10001;
                    3'd1: bitmap = 5'b10001;
                    3'd2: bitmap = 5'b11111;
                    3'd3: bitmap = 5'b00001;
                    3'd4: bitmap = 5'b00001;
                    default: bitmap = 0;
                  endcase
            4'd5: case(row)
                    3'd0: bitmap = 5'b11111;
                    3'd1: bitmap = 5'b10000;
                    3'd2: bitmap = 5'b11110;
                    3'd3: bitmap = 5'b00001;
                    3'd4: bitmap = 5'b11110;
                    default: bitmap = 0;
                  endcase
            4'd6: case(row)
                    3'd0: bitmap = 5'b01110;
                    3'd1: bitmap = 5'b10000;
                    3'd2: bitmap = 5'b11110;
                    3'd3: bitmap = 5'b10001;
                    3'd4: bitmap = 5'b01110;
                    default: bitmap = 0;
                  endcase
            4'd7: case(row)
                    3'd0: bitmap = 5'b11111;
                    3'd1: bitmap = 5'b00001;
                    3'd2: bitmap = 5'b00010;
                    3'd3: bitmap = 5'b00100;
                    3'd4: bitmap = 5'b00100;
                    default: bitmap = 0;
                  endcase
            4'd8: case(row)
                    3'd0: bitmap = 5'b01110;
                    3'd1: bitmap = 5'b10001;
                    3'd2: bitmap = 5'b01110;
                    3'd3: bitmap = 5'b10001;
                    3'd4: bitmap = 5'b01110;
                    default: bitmap = 0;
                  endcase
            4'd9: case(row)
                    3'd0: bitmap = 5'b01110;
                    3'd1: bitmap = 5'b10001;
                    3'd2: bitmap = 5'b01111;
                    3'd3: bitmap = 5'b00001;
                    3'd4: bitmap = 5'b01110;
                    default: bitmap = 0;
                  endcase
            default: bitmap = 5'b00000;
            4'd10: case(row)
                    3'd0: bitmap = 5'b10001;
                    3'd1: bitmap = 5'b01010;
                    3'd2: bitmap = 5'b00100;
                    3'd3: bitmap = 5'b01010;
                    3'd4: bitmap = 5'b10001;
                    default: bitmap = 0;
                  endcase
            default: bitmap = 5'b00000;
            4'd15: case(row)
                    3'd0: bitmap = 5'b00000;
                    3'd1: bitmap = 5'b00000;
                    3'd2: bitmap = 5'b00000;
                    3'd3: bitmap = 5'b00000;
                    3'd4: bitmap = 5'b00000;
                    default: bitmap = 0;
                  endcase
            default: bitmap = 5'b00000;
        endcase
    end
endmodule