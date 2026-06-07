/********************************************
*       Spec 3b Switch Debounce & Toggle  *
*       Authors: Ken Navarro     *
*       Modified by: Aitazaz, Fatin					*
********************************************/
//Switch Logic Subsystem

#include "switches.h"

// Priority window duration responsible for UART light commands measured in milliseconds
#define UART_LIGHT_PRIORITY_MS 10000U

// Use unsigned subtraction so that the check still operates when g_msTick wraps around
#define TIME_ELAPSED(deadline)  ((g_msTick - (deadline)) < 0x80000000U)

// Switch input macros, switch active low therefore pressing = 0
#define SW_LIGHT_PRESSED()  (!(GPIOA->IDR & GPIO_IDR_ID10))  // PA10 - SW4 light switch
#define SW_LUX_PRESSED()    (!(GPIOA->IDR & GPIO_IDR_ID8))   // PA8  - SW2 light intensity sensor
#define SW_FAN_PRESSED()    (!(GPIOB->IDR & GPIO_IDR_ID0))   // PB0  - SW5 fan switch

// Millisecond counter incremented by SysTick every 1ms
volatile uint32_t g_msTick = 0U;

// Debounce for light and fan switches
//lockoutEnd initialised to max value so that no lockout is active on the first boot
static SwitchCtx_t s_lightSw = {1U, 0U, 0U, 0xFFFFFFFFU};
static SwitchCtx_t s_fanSw   = {1U, 0U, 0U, 0xFFFFFFFFU};

// Internal output state variables
static uint8_t s_lightEnabled = LIGHT_INITAL_STATE;
static uint8_t s_fanEnabled   = FAN_INITIAL_STATE;

// Variables for tracking the UART light command priority window
static uint32_t s_uartLightOffTime  = 0U;
static uint8_t  s_uartLightOffValid = 0U;


//=====================================FUNCTION FAN SET START=====================================//
// Function: FAN_SET
// Description: Sets the fan hardware output and updates the internal state variable
// Input: state - 1 to turn fan on, 0 to turn fan off
// Output: None
//=====================================FUNCTION FAN SET END=====================================//
void FAN_SET(uint8_t state)
{
    (void)(state ? (GPIOB->ODR &= ~(GPIO_ODR_OD1_Msk)) : (GPIOB->ODR |= GPIO_ODR_OD1_Msk));    // Active low output
    s_fanEnabled = state;
}


//=====================================FUNCTION LIGHT SET START=====================================//
// Function: LIGHT_SET
// Description: Sets the light hardware output and updates the internal state variable
// Input: state - 1 to turn light on, 0 to turn light off
// Output: None
//=====================================FUNCTION LIGHT SET END=====================================//
void LIGHT_SET(uint8_t state)
{
    (void)(state ? (GPIOA->ODR &= ~(GPIO_ODR_OD9_Msk)) : (GPIOA->ODR |= GPIO_ODR_OD9_Msk));    // Active low output
    s_lightEnabled = state;
}


void configureSysTick(void)
{
    SysTick_Config(SystemCoreClock / 1000U);  // 168MHz / 1000 = 1ms tick
}

void SysTick_Handler(void)
{
    g_msTick++;
}


//=====================================FUNCTION POLL SWITCH START=====================================//
// Function: PollSwitch
// Description: Detects a valid button press using falling and rising edge detection
// Input: sw - pointer to the debounce context for this switch
//        currentlyPressed - 1 if button is pressed, 0 if not
// Output: Returns 1 if a valid press was detected, 0 otherwise
//=====================================FUNCTION POLL SWITCH END=====================================//
static uint8_t PollSwitch(SwitchCtx_t *sw, uint8_t currentlyPressed)
{
    uint8_t event = 0U;

    // Falling edge - button is pressed now but was not pressed on the last sample
    if (currentlyPressed && sw->prevIdle)
    {
        if (!sw->falling && TIME_ELAPSED(sw->lockoutEnd))
        {
            sw->falling     = 1U;
            sw->fallingTime = g_msTick;     // record when the button went down
        }
    }

    if (sw->falling)
    {
        // Rising edge - button was pressed on last sample but is now released
        if (!currentlyPressed && !sw->prevIdle)
        {
            uint32_t heldMs = g_msTick - sw->fallingTime;  // how long was it held

            if (heldMs >= DEBOUNCE_MIN_MS)      // must be held for at least 10ms
            {
                event          = 1U;
                sw->lockoutEnd = g_msTick + LOCKOUT_MS;     // start 2 second lockout
            }
            sw->falling = 0U;
        }
        // Noise - released before prevIdle recorded the press
        else if (!currentlyPressed && sw->prevIdle)
        {
            sw->falling = 0U;
        }
    }

    sw->prevIdle = (uint8_t)(!currentlyPressed);    // remember state for next iteration
    return event;
}


//=====================================FUNCTION HMS POLL FAN SWITCH START=====================================//
// Function: HMS_Poll_Fan_Switch
// Description: Polls the fan switch and toggles the fan state on a valid press.
// Input: o_fan_status    - pointer to fan output status in the outputs struct
//        hardware_update - 1 to drive hardware directly, 0 to only update the flag
// Output: Returns 1 if there is a valid press was detected, 0 otherwise
//=====================================FUNCTION HMS POLL FAN SWITCH END=====================================//
uint8_t HMS_Poll_Fan_Switch(volatile uint8_t *o_fan_status, uint8_t hardware_update)
{
    uint8_t fanPressed = SW_FAN_PRESSED();

    if (PollSwitch(&s_fanSw, fanPressed))
    {
        s_fanEnabled  = (uint8_t)(!s_fanEnabled);   // toggle fan state
        *o_fan_status = s_fanEnabled;               // update the external status flag

        if (hardware_update == 1)
        {
            if (s_fanEnabled)
                FAN_ON();
            else
                FAN_OFF();
        }

        return 1;   // valid press found
    }

    return 0;   // no press found
}


//=====================================FUNCTION HMS POLL LIGHT SWITCH START=====================================//
// Function: HMS_Poll_Light_Switch
// Description: Polls the light switch with two additional checks before toggling.
//              Check 1 - UART priority
//              Check 2 - Lux sensor
// Input: o_light_status - pointer to light output status in the outputs struct
// Output: None
//=====================================FUNCTION HMS POLL LIGHT SWITCH END=====================================//
void HMS_Poll_Light_Switch(volatile uint8_t *o_light_status)
{
    uint8_t lightPressed = SW_LIGHT_PRESSED();

    if (PollSwitch(&s_lightSw, lightPressed))
    {
        // Check 1 - UART priority window. Suppress switch if UART commanded recently.
        if (s_uartLightOffValid && ((g_msTick - s_uartLightOffTime) <= UART_LIGHT_PRIORITY_MS))
        {
            s_lightEnabled  = 0U;
            *o_light_status = 0U;
            LIGHT_OFF();
            return;
        }

        // Check 2 - Light intensity sensor. Only check SW2 if the light is currently off.
        if (s_lightEnabled == 0U)
        {
            if (SW_LUX_PRESSED())
            {
                // SW2 is held - room is already lit, block turning the light on
                s_lightEnabled  = 0U;
                *o_light_status = 0U;
                LIGHT_OFF();
            }
            else
            {
                // SW2 not held - allow the light to turn on
                s_lightEnabled  = 1U;
                *o_light_status = 1U;
                LIGHT_ON();
            }
        }
        else
        {
            // Light is already on - always allow turning it off
            s_lightEnabled  = 0U;
            *o_light_status = 0U;
            LIGHT_OFF();
        }
    }
}


//=====================================FUNCTION HMS UART SET LIGHT START=====================================//
// Function: HMS_UART_Set_Light
// Description: Applies a UART light command 
// Input: o_light_status   - pointer to light output status in the outputs struct
//        uartLightCommand - 1 to turn light on, 0 to turn light off
// Output: None
//=====================================FUNCTION HMS UART SET LIGHT END=====================================//
void HMS_UART_Set_Light(volatile uint8_t *o_light_status, uint8_t uartLightCommand)
{
    if (uartLightCommand)
    {
        // UART commanded light ON - apply directly, SW2 check is bypassed
        s_lightEnabled  = 1U;
        *o_light_status = 1U;
        LIGHT_ON();
    }
    else
    {
        // UART commanded light OFF - apply and record timestamp for priority window
        s_lightEnabled  = 0U;
        *o_light_status = 0U;
        LIGHT_OFF();

        s_uartLightOffTime  = g_msTick;
        s_uartLightOffValid = 1U;
    }
}