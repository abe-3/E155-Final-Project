module hit_detector(
    input logic clk, reset,
    input logic [3:0] sync_drum_beat,
    input logic [3:0] lane_active_perfect,
    input logic [3:0] lane_active_okay,
    output logic [3:0] hit_perfect,
    output logic [3:0] hit_okay,
    output logic [3:0] hit_miss
);

    integer i;

    always_ff @(posedge clk) begin
        if (reset) begin
            hit_perfect <= '0;
            hit_okay <= '0;
            hit_miss <= '0;
        end else begin
            for (i = 0; i < 4; i = i + 1) begin
                hit_perfect[i] <= '0;
                hit_okay[i] <= '0;
                hit_miss[i] <= '0;

                if (sync_drum_beat) begin
                    if (lane_active_perfect[i]) begin
                        hit_perfect[i] <= 1'b1;
                    end else if (lane_active_okay[i]) begin
                        hit_okay[i] <= 1'b1;
                    end else begin
                        hit_miss[i] <= 1'b1;
                    end
                end
            end
        end
    end
endmodule