/********************************************
*       Spec 3b Switch Debounce & Toggle  *
*       Authors: Ken Navarro     *
*       Modified by: Aitazaz, Fatin					*
********************************************/

#include "switches.h"

// ============================================================
// TIMING
// ============================================================
#define TIME_ELAPSED(deadline)  ((g_msTick - (deadline)) < 0x80000000U)
#define UART_LIGHT_PRIORITY_MS 10000U

// ============================================================
// GPIO INPUT MACROS (active low pressed = 0 on pin = 1 here)
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
// POLLSWITCH debounce and lockout logic
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
// HMS_POLL_FAN_SWITCH pure toggle logic oly for fan
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
// HMS_POLL_LIGHT_SWITCH pure toggle logic only for light
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