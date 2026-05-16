/********************************************
*       Spec 3b � Switch Debounce & Toggle  *
*       Authors: Ken Navarro     *
********************************************/

#include "switches.h"

// ============================================================
// TIMING
// ============================================================
#define TIME_ELAPSED(deadline)  ((g_msTick - (deadline)) < 0x80000000U)
#define UART_LIGHT_PRIORITY_MS 10000U

// ============================================================
// GPIO INPUT MACROS (active low � pressed = 0 on pin = 1 here)
// ============================================================
#define SW_LIGHT_PRESSED()  (!(GPIOA->IDR & GPIO_IDR_ID10))  // PA10 SW4
#define SW_LUX_PRESSED()    (!(GPIOA->IDR & GPIO_IDR_ID8))   // PA8  SW2
#define SW_FAN_PRESSED()    (!(GPIOB->IDR & GPIO_IDR_ID0))   // PB0  SW5


volatile uint32_t g_msTick = 0U;

static SwitchCtx_t s_lightSw = {1U, 0U, 0U, 0xFFFFFFFFU};
static SwitchCtx_t s_fanSw   = {1U, 0U, 0U, 0xFFFFFFFFU};

// ============================================================
// OUTPUT STATE
// ============================================================
static uint8_t s_lightEnabled = LIGHT_INITAL_STATE;
static uint8_t s_fanEnabled   = FAN_INITIAL_STATE;           // Default on. Match AUTO MODE state.


static uint32_t s_uartLightOffTime = 0U;
static uint8_t s_uartLightOffValid = 0U;


// ============================================================
// Use set funcs to keep one source of truth for hardware state of FAN.
// The func will update the internal state kept in switches. 
// ============================================================
void FAN_SET(uint8_t state)
{
    (void)(state ? (GPIOB->ODR &= ~(GPIO_ODR_OD1_Msk)) : ( GPIOB->ODR |= GPIO_ODR_OD1_Msk));    // state = 0 or 1. Active low output.
    s_fanEnabled = state;
}

// ============================================================
// Use set funcs to keep one source of truth for hardware state of Light.
// The func will update the internal state kept in switches. 
// ============================================================
void LIGHT_SET(uint8_t state)
{
    (void)(state ? (GPIOA->ODR &= ~(GPIO_ODR_OD9_Msk)) : ( GPIOA->ODR |= GPIO_ODR_OD9_Msk));    // state = 0 or 1. Active low output.
    s_lightEnabled = state;
}


// ============================================================
// RCC � enable clocks for GPIOA and GPIOB
// ============================================================
void configureRCC_SW(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    RCC->AHB1RSTR |= RCC_AHB1RSTR_GPIOARST | RCC_AHB1RSTR_GPIOBRST;
    __asm("NOP"); __asm("NOP");
    RCC->AHB1RSTR &= ~(RCC_AHB1RSTR_GPIOARST | RCC_AHB1RSTR_GPIOBRST);
    __asm("NOP"); __asm("NOP");
}


// ============================================================
// GPIO � configure only the pins needed for 3b
// PA8  input  (SW2 lux sensor)
// PA9  output (LED2 light)
// PA10 input  (SW4 light switch)
// PB0  input  (SW5 fan switch)
// PB1  output (LED5 fan)
// ============================================================
void configureGPIO_SW(void)
{
    // GPIOA
    GPIOA->MODER &= ~(GPIO_MODER_MODER10_Msk |
                      GPIO_MODER_MODER9_Msk  |
                      GPIO_MODER_MODER8_Msk);
    GPIOA->MODER |=  (0x01U << GPIO_MODER_MODE9_Pos);  // PA9 output

    GPIOA->OTYPER &= ~(GPIO_OTYPER_OT10 | GPIO_OTYPER_OT8);
    GPIOA->OTYPER |=  (0x01U << GPIO_OTYPER_OT9_Pos);

    GPIOA->OSPEEDR |= (0x03U << GPIO_OSPEEDR_OSPEED10_Pos) |
                      (0x03U << GPIO_OSPEEDR_OSPEED9_Pos)  |
                      (0x03U << GPIO_OSPEEDR_OSPEED8_Pos);

    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD10_Msk |
                      GPIO_PUPDR_PUPD9_Msk  |
                      GPIO_PUPDR_PUPD8_Msk);
    GPIOA->PUPDR |=  (0x01U << GPIO_PUPDR_PUPD10_Pos) |  // PA10 pull-up
                     (0x01U << GPIO_PUPDR_PUPD8_Pos);     // PA8  pull-up

    // GPIOB
    GPIOB->MODER &= ~(GPIO_MODER_MODER1_Msk |
                      GPIO_MODER_MODER0_Msk);
    GPIOB->MODER |=  (0x01U << GPIO_MODER_MODE1_Pos);    // PB1 output

    GPIOB->OTYPER &= ~(GPIO_OTYPER_OT0);
    GPIOB->OTYPER |=  (0x01U << GPIO_OTYPER_OT1_Pos);

    GPIOB->OSPEEDR |= (0x03U << GPIO_OSPEEDR_OSPEED1_Pos) |
                      (0x03U << GPIO_OSPEEDR_OSPEED0_Pos);

    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD1_Msk |
                      GPIO_PUPDR_PUPD0_Msk);
    GPIOB->PUPDR |=  (0x01U << GPIO_PUPDR_PUPD0_Pos);    // PB0 pull-up
}


// ============================================================
// SYSTICK SETUP
// ============================================================
void configureSysTick(void)
{
    SysTick_Config(SystemCoreClock / 1000U);
}
void SysTick_Handler(void)
{
    g_msTick++;
}
// ============================================================
// POLLSWITCH � debounce and lockout logic
// Returns 1 on valid press, 0 otherwise
// ============================================================
static uint8_t PollSwitch(SwitchCtx_t *sw, uint8_t currentlyPressed)
{
    uint8_t event = 0U;

    // Falling edge: pressed now, was idle before
    if (currentlyPressed && sw->prevIdle)
    {
        if (!sw->falling && TIME_ELAPSED(sw->lockoutEnd))
        {
            sw->falling     = 1U;
            sw->fallingTime = g_msTick;
        }
    }

    if (sw->falling)
    {
        // Rising edge: released after being recorded as pressed
        if (!currentlyPressed && !sw->prevIdle)
        {
            uint32_t heldMs = g_msTick - sw->fallingTime;
            if (heldMs >= DEBOUNCE_MIN_MS)
            {
                event          = 1U;
                sw->lockoutEnd = g_msTick + LOCKOUT_MS;
            }
            sw->falling = 0U;
        }
        // Noise: released before prevIdle recorded the press
        else if (!currentlyPressed && sw->prevIdle)
        {
            sw->falling = 0U;
        }
    }

    sw->prevIdle = (uint8_t)(!currentlyPressed);
    return event;
}


// ============================================================
// HMS_POLL_FAN_SWITCH � pure toggle logic only for fan
// ============================================================
// Also updates the fan and light status flags provided. This is because the internal flags are private.

uint8_t HMS_Poll_Fan_Switch(volatile uint8_t *o_fan_status, uint8_t hardware_update)
{
    uint8_t fanPressed   = SW_FAN_PRESSED();

    // Fan switch toggle
    if (PollSwitch(&s_fanSw, fanPressed))
    {
        s_fanEnabled = (uint8_t)(!s_fanEnabled);
				*o_fan_status = s_fanEnabled;																				// Update the external status flag.
        if (hardware_update == 1)
				{
					if (s_fanEnabled)
							FAN_ON();
					else
							FAN_OFF();
				}
				
				return 1;																														// Return 1 if successfully found a change in switch
    }
		
		return 0;																																// Return 0 if no change in switch found.
}


// ============================================================
// HMS_POLL_LIGHT_SWITCH � pure toggle logic only for light
// ============================================================
// Also updates the fan and light status flags provided. This is because the internal flags are private.

void HMS_Poll_Light_Switch(volatile uint8_t *o_light_status)
{
    uint8_t lightPressed = SW_LIGHT_PRESSED();

    if (PollSwitch(&s_lightSw, lightPressed))
    {
        // UART light-off gets priority for 1 second.
        if (s_uartLightOffValid && ((g_msTick - s_uartLightOffTime) <= UART_LIGHT_PRIORITY_MS))
        {
            s_lightEnabled = 0U;
            *o_light_status = 0U;
            LIGHT_OFF();
            return;
        }

        // If light is off, only allow SW4 to turn it on when SW2 is not held.
        if (s_lightEnabled == 0U)
        {
            if (SW_LUX_PRESSED())
            {
                s_lightEnabled = 0U;
                *o_light_status = 0U;
                LIGHT_OFF();
            }
            else
            {
                s_lightEnabled = 1U;
                *o_light_status = 1U;
                LIGHT_ON();
            }
        }
        else
        {
            s_lightEnabled = 0U;
            *o_light_status = 0U;
            LIGHT_OFF();
        }
    }
}

void HMS_UART_Set_Light(volatile uint8_t *o_light_status, uint8_t uartLightCommand)
{
    // UART ignores SW2 light sensor.
    if (uartLightCommand)
    {
        s_lightEnabled = 1U;
        *o_light_status = 1U;
        LIGHT_ON();
    }
    else
    {
        s_lightEnabled = 0U;
        *o_light_status = 0U;
        LIGHT_OFF();

        s_uartLightOffTime = g_msTick;
        s_uartLightOffValid = 1U;
    }
}