module debouncer #(
    parameter CLK_FREQ = 48000000,
    parameter LOCKOUT_MS = 10
) (
    input logic clk, reset, unsync_hit,
    output logic sync_hit
);

    localparam int WAIT_CYCLES = (CLK_FREQ / 1000) * LOCKOUT_MS;
    localparam int CTR_WIDTH = $clog2(WAIT_CYCLES);

    logic [CTR_WIDTH-1:0] counter;
    logic sync_0, sync_1;
    logic rising_edge;

    always_ff @(posedge clk) begin
        if (reset) begin
            sync_0 <= 0;
            sync_1 <= 0;
        end else begin
            sync_0 <= unsync_hit;
            sync_1 <= sync_0;
        end
    end
    
    assign rising_edge = sync_0 && ~sync_1;

    // Debounce Lockout
    always_ff @(posedge clk) begin
        if (reset) begin
            counter <= 0;
            sync_hit <= 0;
        end else begin
            sync_hit <= 0;

            if (counter > 0) begin
                counter <= counter - 1;
            end else if (rising_edge) begin
                sync_hit <= 1;
                counter <= WAIT_CYCLES;
            end
        end
    end

endmodule