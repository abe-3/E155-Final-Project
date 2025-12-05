// STM32L432KC_DAC.c
#include "STM32L432KC_DAC.h"
#include "STM32L432KC_RCC.h"
#include "STM32L432KC_GPIO.h"

void Audio_DAC_Init(void) {
    // 1. Enable DAC Clock
    RCC->APB1ENR1 |= RCC_APB1ENR1_DAC1EN;

    // 2. Configure GPIO PA5 as Analog (DAC1_OUT2)
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    pinMode(PA5, GPIO_ANALOG);

    // 3. Configure DAC Mode: Normal Mode with Output Buffer
    // Bits 18:16 for Channel 2 in DAC_MCR
    // 000: Connected to external pin with Buffer enabled (Default, but let's be explicit)
    DAC1->MCR &= ~(DAC_MCR_MODE2); 

    // 4. Enable DAC Channel 2
    DAC1->CR |= DAC_CR_EN2; 
    
    // Wait for stabilization (tWAKEUP ~15us)
    volatile int i;
    for(i=0; i<1000; i++); 
}

void Audio_Timer_Init(uint32_t sampleRate) {
    // 1. Enable TIM6 Clock
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM6EN;

    // 2. Configure Period
    // F_timer = 80 MHz
    // ARR = (80,000,000 / sampleRate) - 1
    uint32_t arrValue = (SystemCoreClock / sampleRate) - 1;

    TIM6->PSC = 0;
    TIM6->ARR = arrValue;

    // 3. Enable Update Interrupt
    TIM6->DIER |= TIM_DIER_UIE;

    // 4. Enable Interrupt in NVIC
    NVIC_EnableIRQ(TIM6_DAC_IRQn);
    NVIC_SetPriority(TIM6_DAC_IRQn, 0); // High priority

    // 5. Enable Timer
    TIM6->CR1 |= TIM_CR1_CEN;
}