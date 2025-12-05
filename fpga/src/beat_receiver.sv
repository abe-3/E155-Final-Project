// beat_receiver.sv
// Receives 1 byte from STM32. Bottom 4 bits = lane mask.
module beat_receiver (
    input  logic clk,       // System clock (48MHz)
    input  logic reset,
    input  logic sck,       // SPI Clock (from STM32)
    input  logic sdi,       // SPI MOSI (from STM32)
    input  logic cs_n,      // SPI Chip Select (from STM32 PB0)
    output logic [3:0] lane_mask,  // The decoded beat (to pattern_gen)
    output logic new_beat          // Pulse when new data arrives
);

    logic [7:0] shift_reg;
    logic [3:0] bit_count;
    logic ready_sck;

    // --- SPI Domain (Fast/Async) ---
    always_ff @(posedge sck or posedge cs_n) begin
        if (cs_n) begin
            bit_count <= '0;
            ready_sck <= '0;
        end else begin
            shift_reg <= {shift_reg[6:0], sdi}; // Shift MSB first
            bit_count <= bit_count + 1;
            
            // Trigger after 8 bits (0-7)
            if (bit_count == 7) begin
                ready_sck <= 1'b1;
            end else begin
                ready_sck <= 1'b0;
            end
        end
    end
    
    // --- System Clock Domain (Synchronizer) ---
    logic [2:0] sync_pipe;

    always_ff @(posedge clk) begin
        if (reset) begin
            sync_pipe <= '0;
            lane_mask <= '0;
            new_beat  <= 0;
        end else begin
            sync_pipe <= {sync_pipe[1:0], ready_sck};

            // Rising edge detection of the synced signal
            if (sync_pipe[1] && ~sync_pipe[2]) begin
                new_beat  <= 1'b1;
                lane_mask <= shift_reg[3:0]; // Capture lanes
            end else begin
                new_beat <= 0;
                // Optional: Clear lane_mask after 1 cycle if you want pulses
                // But keeping it latched usually looks better for debug
            end
        end
    end

endmodule