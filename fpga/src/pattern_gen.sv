// Abraham Rock
// arock@hmc.edu
// November 14, 2025

module pattern_gen #(
	parameter DATA_WIDTH = 24,	// 8 bits per color
	parameter ADDR_WIDTH = 12,
	parameter M_W = 64,			// Matrix Width and Height
	parameter M_H = 64) (
	input logic clk, reset,
	input logic [3:0] drum_beat, song_beat,
	output logic w_en,
	output logic [ADDR_WIDTH-1:0] w_addr,
	output logic [DATA_WIDTH-1:0] w_data,
	output logic frame_done,
	output logic [3:0] target_perfect, target_okay);
	
	// Coordinates
	logic [5:0] x_coord;
	logic [5:0] y_coord, y_virtual;
    assign y_virtual = y_coord + 6'd32;		// Half Plane Offset
	
	// Timing
	// 48 MHz / 1.6 MHz = 30 Shifts per second
	localparam SHIFT_THRESHOLD = 800000;
	localparam FB_DURATION = 12000000;		// Duration to show if perfect or okay

	logic [31:0] timer, fb_timers[3:0];
	logic [1:0] fb_states [3:0];	// 0: None, 1: Perfect, 2: Okay, 3: Miss
	logic [15:0] total_score;
	
	// Lane and Color Logic
	logic [63:0] lane_0, lane_1, lane_2, lane_3;
	logic [23:0] pixel_color;
	logic [1:0] current_lane;
	logic current_bit;
	
	// Input Sync
	logic [3:0] drum_beat_sync, drum_beat_prev;
	logic [3:0] song_beat_sync, song_beat_prev;

	// Scoring
	logic [3:0] digit_ones, digit_tens, digit_hundreds;
    logic [3:0] current_digit;
    logic [4:0] rom_bitmap;
    logic is_score_pixel;
    logic [2:0] x_rel_score;

	assign digit_ones = (total_score) % 10;
	assign digit_tens = (total_score / 10) % 10;
	assign digit_hundreds = (total_score / 100) % 10;

	// Score Digit Selector
	always_comb begin
		current_digit = 0;
		x_rel_score = 0;

		if (x_coord >= 58 && x_coord <= 62) begin
			current_digit = digit_ones;
            x_rel_score = x_coord - 6'd58;
        end else if (x_coord >= 52 && x_coord <= 56) begin
			current_digit = digit_tens;
            x_rel_score = x_coord - 6'd52;
        end else if (x_coord >= 46 && x_coord <= 50) begin
			current_digit = digit_hundreds;
            x_rel_score = x_coord - 6'd46;
        end
	end

	font_rom scoreboard_font(
		.digit(current_digit),
		.row(y_coord[2:0] - 3'd1),
		.bitmap(rom_bitmap)
	);

	always_comb begin
        if (y_coord >= 1 && y_coord <= 5) begin
            if ((x_coord >= 58 && x_coord <= 62) || 
                (x_coord >= 52 && x_coord <= 56) || 
                (x_coord >= 46 && x_coord <= 50)) begin
                // Map MSB of bitmap to left-most pixel of digit
                is_score_pixel = rom_bitmap[4 - x_rel_score];
            end else begin
                is_score_pixel = 0;
            end
        end else begin
            is_score_pixel = 0;
        end
	end

	always_comb begin
		target_perfect[0] =	|lane_0[63:61];
		target_okay[0] = 	|lane_0[63:57];
		target_perfect[1] = |lane_1[63:61];
		target_okay[1] = 	|lane_1[63:57];
		target_perfect[2] = |lane_2[63:61];
		target_okay[2] = 	|lane_2[63:57];
		target_perfect[3] = |lane_3[63:61];
		target_okay[3] = 	|lane_3[63:57];
	end
	
	// Falling and Trigger Logic
	always_ff @(posedge clk) begin
		logic [3:0] score_inc;
		if (reset == 1) begin
			timer <= '0;
			lane_0 <= '0; lane_1 <= '0; lane_2 <= '0; lane_3 <= '0;
			drum_beat_sync <= '0; song_beat_sync <= '0;
			drum_beat_prev <= '0; song_beat_prev <= '0;
			fb_timers[0] <= '0; fb_states[0] <= '0;
			fb_timers[1] <= '0; fb_states[1] <= '0;
			fb_timers[2] <= '0; fb_states[2] <= '0;
			fb_timers[3] <= '0; fb_states[3] <= '0;
			digit_ones <= 0;
            digit_tens <= 0;
            digit_hundreds <= 0;
		end else begin
			drum_beat_sync <= drum_beat;
			drum_beat_prev <= drum_beat_sync;
			song_beat_sync <= song_beat;
			song_beat_prev <= song_beat_sync;

			// Default
			score_inc = 0;
			
			// Shifting Logic
			if (timer >= SHIFT_THRESHOLD) begin
				timer <= '0;
				lane_0 <= {lane_0[62:0], 1'b0};
				lane_1 <= {lane_1[62:0], 1'b0};
				lane_2 <= {lane_2[62:0], 1'b0};
				lane_3 <= {lane_3[62:0], 1'b0};
			end else begin
				timer <= timer + 1;
			end
			
			// Trigger Logic
			if (song_beat_sync[0] && ~song_beat_prev[0]) lane_0[1:0] <= 2'b11;
			if (song_beat_sync[1] && ~song_beat_prev[1]) lane_1[1:0] <= 2'b11;
			if (song_beat_sync[2] && ~song_beat_prev[2]) lane_2[1:0] <= 2'b11;
			if (song_beat_sync[3] && ~song_beat_prev[3]) lane_3[1:0] <= 2'b11;

			// Lane 0
			if (fb_timers[0] > 0) fb_timers[0] <= fb_timers[0] - 1;
			if (drum_beat_sync[0] && ~drum_beat_prev[0]) begin
				fb_timers[0] <= FB_DURATION;
				if (|lane_0[63:61]) begin fb_states[0] <= 1; score_inc = score_inc + 3; end		// Perfect
				else if (|lane_0[63:57]) begin fb_states[0] <= 2; score_inc = score_inc + 1; end	// Okay
				else fb_states[0] <= 3;							// Miss
			end

			// Lane 1
			if (fb_timers[1] > 0) fb_timers[1] <= fb_timers[1] - 1;
			if (drum_beat_sync[1] && ~drum_beat_prev[1]) begin
				fb_timers[1] <= FB_DURATION;
				if (|lane_1[63:61]) begin fb_states[1] <= 1; score_inc = score_inc + 3; end		// Perfect
				else if (|lane_1[63:57]) begin fb_states[1] <= 2; score_inc = score_inc + 1; end	// Okay
				else fb_states[1] <= 3;							// Miss
			end

			// Lane 2
			if (fb_timers[2] > 0) fb_timers[2] <= fb_timers[2] - 1;
			if (drum_beat_sync[2] && ~drum_beat_prev[2]) begin
				fb_timers[2] <= FB_DURATION;
				if (|lane_2[63:61]) begin fb_states[2] <= 1; score_inc = score_inc + 3; end		// Perfect
				else if (|lane_2[63:57]) begin fb_states[2] <= 2; score_inc = score_inc + 1; end	// Okay
				else fb_states[2] <= 3;																	// Miss
			end

			// Lane 3
			if (fb_timers[3] > 0) fb_timers[3] <= fb_timers[3] - 1;
			if (drum_beat_sync[3] && ~drum_beat_prev[3]) begin
				fb_timers[3] <= FB_DURATION;
				if (|lane_3[63:61]) begin fb_states[3] <= 1; score_inc = score_inc + 3; end		// Perfect
				else if (|lane_3[63:57]) begin fb_states[3] <= 2; score_inc = score_inc + 1; end	// Okay
				else fb_states[3] <= 3;																	// Miss
			end
			
			// BCD Score Update
            if (score_inc > 0) begin
                logic [4:0] next_ones;
                next_ones = digit_ones + score_inc;
                
                if (next_ones >= 20) begin
                    digit_ones <= next_ones - 5'd20;
                    digit_tens <= (digit_tens >= 8) ? (digit_tens - 4'd8) : (digit_tens + 4'd2);
                    if (digit_tens >= 8) digit_hundreds <= digit_hundreds + 1;
                end else if (next_ones >= 10) begin
                    digit_ones <= next_ones - 5'd10;
                    digit_tens <= (digit_tens == 9) ? 4'd0 : (digit_tens + 4'd1);
                    if (digit_tens == 9) digit_hundreds <= digit_hundreds + 1;
                end else begin
                    digit_ones <= next_ones[3:0];
                end
            end
		end
	end
	
	// Render Logic
	always_comb begin
		current_bit = 1'b0;
		
		// Lane Logic
		case (current_lane)
			2'd0: current_bit = lane_0[y_virtual];
			2'd1: current_bit = lane_1[y_virtual];
			2'd2: current_bit = lane_2[y_virtual];
			2'd3: current_bit = lane_3[y_virtual];
			default: current_bit = '0;
		endcase
		
		// Lanes
		if (x_coord[3:0] == 4'd0) begin
			pixel_color = 24'h000000;		// Black Gap
		end else if (current_bit) begin
			pixel_color = 24'hFFFFFF;		// White Lane
		end else begin
			pixel_color = 24'h000000;
		end

		// Feedback Override
		if (y_virtual >= 56) begin
			if (fb_timers[current_lane] > 0) begin
				case (fb_states[current_lane])
					1: pixel_color = 24'h00FF00;
					2: pixel_color = 24'hFFFF00;
					3: pixel_color = 24'hFF0000;
					default: pixel_color = 24'hFFFFFF;
				endcase
			end else if (y_virtual == 61) begin
				// Hit Line
				if (~current_bit) pixel_color = 24'h202020;
			end
		end

		// Scoreboard Override
        if (is_score_pixel) pixel_color = 24'h00FFFF;
	end
	
	// Write Logic
	always_ff @(posedge clk) begin
		if (reset == 1) begin
			x_coord <= 0;
			y_coord <= 0;
			w_en <= 0;
			w_addr <= 0;
			w_data <= '0;
		end else begin
			// writes a pixel
			w_addr <= {y_coord, x_coord}; 
			w_data <= pixel_color;
			w_en <= 1;
			frame_done <= 0;
			
			// increment coordinates
			if (x_coord == M_W - 1) begin
				x_coord <= 0;
				if (y_coord == M_H - 1) begin
					y_coord <= 0;
					frame_done <= 1;
				end else begin
					y_coord <= y_coord + 1'b1;
				end
			end else begin
				x_coord <= x_coord + 1'b1;
			end
		end
	end
	
	assign current_lane = x_coord[5:4];
endmodule