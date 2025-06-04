#ifndef SW_PWM_H
#define SW_PWM_H

#include "stm32l0xx_hal.h" // For GPIO_TypeDef, uint16_t, etc.
#include <stdint.h>

/* Constants */
#define NUM_SW_PWM_CHANNELS 6
#define SW_PWM_RESOLUTION 20

/* Function Prototypes */
void init_software_pwm(void);
void update_software_pwm(void); // Called by SysTick_Handler
void set_sw_pwm_channel_duty(uint8_t sw_channel_idx, uint8_t duty_0_to_resolution);
const uint8_t* get_sw_pwm_pin_indices(void); // Getter for pin indices

#endif // SW_PWM_H
