# E155-Final-Project: DDRUM, an Embedded Rhythm Game

**DDRUM** is a mixed-signal embedded rhythm game system that integrates real-time audio processing with high-speed hardware acceleration. The system uses an STM32 microcontroller for audio streaming and beat analysis, while an iCE40 FPGA handles the parallel graphics rendering and input processing.

## Project Overview

* **Platform:** STM32L432KC (MCU) + Lattice iCE40UP5K (FPGA)
* **Display:** 64x64 RGB LED Matrix (HUB75 Interface)
* **Input:** 4x Custom Piezoelectric Drum Pads
* **Storage:** Micro SD Card (.wav file playback)

## Repository Structure

The repository is divided into two main directories corresponding to the two processing units:

```text
.
├── mcu/                  # Firmware for STM32L432KC
│   ├── main.c            # Circular buffer, game loop, beat detection
│   ├── STM32L432KC_SD.c  # SD Card & FAT32 driver
│   ├── STM32L432KC_DAC.c # Audio output driver
│   ├── STM32L432KC_SPI.c # Communication with FPGA & SD
│   └── ...
├── fpga/                 # Gateware for iCE40UP5K
│   ├── top.sv            # Top-level integration
│   ├── pattern_gen.sv    # Game engine (falling notes, hit lines)
│   ├── beat_receiver.sv  # SPI Slave interface
│   ├── hub75_top.v       # LED Matrix Driver (BCM)
│   ├── hit_detector.sv   # Collision detection logic
│   └── ...
└── README.md             # This file