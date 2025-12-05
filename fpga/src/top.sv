// Abraham Rock
// arock@hmc.edu
// November 14, 2025

`default_nettype wire

// Top Level Module
module top #(
	parameter DATA_WIDTH = 24,	// 8 bits per color
	parameter ADDR_WIDTH = 12,
	parameter M_W = 64,			// Matrix Width and Height
	parameter M_H = 64) (
	input logic reset_n,
	input logic [3:0] drum_beat,
	input logic sck, sdi, cs_n,
	output logic [5:0] matrix_data,
	output logic [4:0] matrix_row,
	output logic matrix_clk, matrix_lat, matrix_oe
);
	
	// Internal Signals
	logic [ADDR_WIDTH-1:0] w_addr;
	logic [DATA_WIDTH-1:0] w_data;
	logic reset, int_osc, w_en;
	logic pg_frame_done;
	logic row_done;

	logic [3:0] sync_drum_beat;
	logic [3:0] target_perfect, target_okay;
	logic [3:0] score_perfect, score_okay, score_miss;
	logic [3:0] spi_beat_mask;
	logic spi_new_data;
	
	// Internal high-speed oscillator
	SB_HFOSC #(.CLKHF_DIV("0b01")) 
         hf_osc (
             .CLKHFPU(1'b1), 
             .CLKHFEN(1'b1), 
             .CLKHF(int_osc)
         );
		
	genvar i;
	generate
		for (i = 0; i < 4; i = i + 1) begin : gen_sync
			debouncer #(
				.CLK_FREQ(24000000),
				.LOCKOUT_MS(100)
			) debounce (
				.clk(int_osc),
				.reset(reset),
				.unsync_hit(drum_beat[i]),
				.sync_hit(sync_drum_beat[i])
			);
		end
	endgenerate

	hit_detector game_logic (
		.clk(int_osc),
		.reset(reset),
		.sync_drum_beat(sync_drum_beat),
		.lane_active_perfect(target_perfect),
		.lane_active_okay(target_okay),
		.hit_perfect(score_perfect),
		.hit_okay(score_okay),
		.hit_miss(score_miss)
	);

	beat_receiver spi_inst(
		.clk(int_osc),
		.reset(reset),
		.sck(sck),
		.sdi(sdi),
		.cs_n(cs_n),
		.lane_mask(spi_beat_mask),
		.new_beat(spi_new_data)
	);
	
	// top level HUB75 module from no2hub75
	hub75_top #(
		.N_BANKS(2),
		.N_ROWS(32),
		.N_COLS(64),
		.N_CHANS(3),
		.N_PLANES(8),
		.BITDEPTH(24),
		.PHY_DDR(0))
	led_driver (
		.clk(int_osc),
		.clk_2x(1'b0),
		.rst(reset),
		.ctrl_run(1'b1),
		.cfg_pre_latch_len(8'd10),
		.cfg_latch_len(8'd10),
		.cfg_post_latch_len(8'd10),
		.cfg_bcm_bit_len(8'd100),
		.fbw_bank_addr(w_addr[11]),
		.fbw_row_addr(w_addr[10:6]),
		.fbw_col_addr(w_addr[5:0]),
		.fbw_data(w_data),
		.fbw_wren(w_en),
		.fbw_row_store(row_done),
		.fbw_row_swap(row_done),
		.frame_swap(pg_frame_done),
		.frame_rdy(),
		.fbw_row_rdy(),
		.hub75_clk(matrix_clk),
		.hub75_le(matrix_lat),
		.hub75_blank(matrix_oe),
		.hub75_data(matrix_data),
		.hub75_addr(matrix_row),
		.hub75_addr_inc(),
		.hub75_addr_rst()
	);
		
	pattern_gen #(
		.DATA_WIDTH(DATA_WIDTH),
		.ADDR_WIDTH(ADDR_WIDTH),
		.M_W(M_W),
		.M_H(M_H)) pg (
		.clk(int_osc),
		.reset(reset),
		.drum_beat(sync_drum_beat),
		.song_beat(spi_beat_mask),
		.w_en(w_en),
		.w_addr(w_addr),
		.w_data(w_data),
		.frame_done(pg_frame_done),
		.target_perfect(target_perfect),
		.target_okay(target_okay)
	);
	
	assign reset = ~reset_n;
	assign row_done = w_en && (w_addr[5:0] == 6'd63);
endmodule