// STM32L432KC_DAC.h
// Header for DAC and Audio Timer

#ifndef STM32L4_DAC_H
#define STM32L4_DAC_H

#include "stm32l432xx.h"

// Initialize DAC1 on Channel 2 (PA5)
void Audio_DAC_Init(void);

// Start Timer 6 interrupts at a specific frequency (Hz)
void Audio_Timer_Init(uint32_t sampleRate);

// Write a 12-bit value to the DAC
// Inlined for speed in ISR
static inline void DAC_Write(uint16_t value) {
    // Write to 12-bit right-aligned data holding register for Channel 2
    // Channel 2 corresponds to pin PA5
    DAC1->DHR12R2 = value;
}

#endif