# Project
PROJ      = blinky
BUILD_DIR = ./build

# Source Files
SRC       = src/top.sv \
            src/pattern_gen.sv \
            src/hub75_top.v \
            src/hub75_bcm.v \
            src/hub75_blanking.v \
            src/hub75_colormap.v \
            src/hub75_fb_readout.v \
            src/hub75_fb_writein.v \
            src/hub75_framebuffer.v \
            src/hub75_gamma.v \
            src/hub75_init_inject.v \
            src/hub75_linebuffer.v \
            src/hub75_phy.v \
            src/hub75_scan.v \
            src/hub75_shift.v \
			src/font_rom.sv \
			src/hit_detector.sv \
			src/debouncer.sv \
			src/beat_receiver.sv\

TB        = src/tb_top.sv
PCF       = constraints/constraints.pcf
DEVICE    = up5k
PACKAGE   = sg48

# Toolchain
BIN       = $(HOME)/oss-cad-suite/bin
YOSYS     = $(BIN)/yosys
NEXTPNR   = $(BIN)/nextpnr-ice40
ICEPACK   = $(BIN)/icepack
# Changed to iceprog with sudo based on your successful manual flash
PROG      = sudo $(BIN)/iceprog
IVERILOG  = $(BIN)/iverilog
VVP       = $(BIN)/vvp
SURFER    = surfer

# Targets
# Changed 'all' to depend on 'prog' so it builds AND uploads automatically
all: prog

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Synthesis & Place/Route
$(BUILD_DIR)/$(PROJ).json: $(SRC) | $(BUILD_DIR)
	$(YOSYS) -p "read_verilog -sv $(SRC); synth_ice40 -top top -json $@"

$(BUILD_DIR)/$(PROJ).asc: $(BUILD_DIR)/$(PROJ).json $(PCF)
	$(NEXTPNR) --$(DEVICE) --package $(PACKAGE) --json $< --pcf $(PCF) --asc $@

$(BUILD_DIR)/$(PROJ).bin: $(BUILD_DIR)/$(PROJ).asc
	$(ICEPACK) $< $@

# Upload
prog: $(BUILD_DIR)/$(PROJ).bin
	@echo "Programming..."
	$(PROG) $(BUILD_DIR)/$(PROJ).bin

# Simulation
sim: $(TB) $(SRC) | $(BUILD_DIR)
	$(IVERILOG) -g2012 -DSIMULATION -o $(BUILD_DIR)/$(PROJ).vvp $(TB) $(SRC)
	$(VVP) $(BUILD_DIR)/$(PROJ).vvp
	$(SURFER) $(PROJ).vcd

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all prog sim clean