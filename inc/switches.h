#ifndef SWITCHES_H
#define SWITCHES_H


#include <stdint.h>
#include "boardSupport.h"
#include "stm32f439xx.h"

// ============================================================
// TIMING
// ============================================================
#define DEBOUNCE_MIN_MS     10U
#define LOCKOUT_MS          2000U
#define FAN_OVERRIDE_MS     10000U

// ============================================================
// GPIO OUTPUT MACROS
// ============================================================
#define FAN_ON()            (GPIOB->ODR |=  GPIO_ODR_OD1)    // PB1  LED5
#define FAN_OFF()           (GPIOB->ODR &= ~GPIO_ODR_OD1)
#define LIGHT_ON()          (GPIOA->ODR |=  GPIO_ODR_OD9)    // PA9  LED2
#define LIGHT_OFF()         (GPIOA->ODR &= ~GPIO_ODR_OD9)


// ============================================================
// SWITCH CONTEXT STRUCT
// ============================================================
typedef struct {
    uint8_t  prevIdle;
    uint8_t  falling;
    uint32_t fallingTime;
    uint32_t lockoutEnd;
} SwitchCtx_t;


uint8_t HMS_Poll_Fan_Switch(volatile uint8_t *o_fan_status, uint8_t hardware_update);
void HMS_UART_Set_Light(volatile uint8_t *o_light_status, uint8_t uartLightCommand);
void HMS_Poll_Light_Switch(volatile uint8_t *o_light_status);
void configureSysTick(void);
void configureGPIO_SW(void);
void configureRCC_SW(void);


#endif
