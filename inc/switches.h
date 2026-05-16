#ifndef SWITCHES_H
#define SWITCHES_H


#include <stdint.h>
#include "boardSupport.h"
#include "stm32f439xx.h"

#define LIGHT_INITAL_STATE  0U
#define FAN_INITIAL_STATE   1U  

// ============================================================
// TIMING
// ============================================================
#define DEBOUNCE_MIN_MS     10U
#define LOCKOUT_MS          2000U
#define FAN_OVERRIDE_MS     10000U

// ============================================================
// GPIO OUTPUT MACROS
// ============================================================
// LEDs are active low. Set ODR to 0 to turn on and set to 1 to turn off
#define FAN_ON()            (GPIOB->ODR &=  ~(GPIO_ODR_OD1_Msk))    // PB1  LED5
#define FAN_OFF()           (GPIOB->ODR |=  GPIO_ODR_OD1)
#define LIGHT_ON()          (GPIOA->ODR &=  ~(GPIO_ODR_OD9))        // PA9  LED2
#define LIGHT_OFF()         (GPIOA->ODR |=  GPIO_ODR_OD9)

// If state = 0 (on (1)) then just shift bit to pos. If state = 1 (off (0) then and out bit 
// #define FAN_SET(state)      (state ? (GPIOB->ODR &= ~(GPIO_ODR_OD1_Msk)) : ( GPIOB->ODR |= GPIO_ODR_OD1_Msk))    // state = 0 or 1. Active low output.
// #define LIGHT_SET(state)    (state ? (GPIOA->ODR &= ~(GPIO_ODR_OD9_Msk)) : ( GPIOA->ODR |= GPIO_ODR_OD9_Msk))    // state = 0 or 1. Active low output.


// ============================================================
// SWITCH CONTEXT STRUCT
// ============================================================
typedef struct {
    uint8_t  prevIdle;
    uint8_t  falling;
    uint32_t fallingTime;
    uint32_t lockoutEnd;
} SwitchCtx_t;


void FAN_SET(uint8_t state);
void LIGHT_SET(uint8_t state);

uint8_t HMS_Poll_Fan_Switch(volatile uint8_t *o_fan_status, uint8_t hardware_update);
void HMS_UART_Set_Light(volatile uint8_t *o_light_status, uint8_t uartLightCommand);
void HMS_Poll_Light_Switch(volatile uint8_t *o_light_status);
void configureSysTick(void);


#endif
