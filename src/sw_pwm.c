#include "sw_pwm.h"
#include "led_control.h" // For LIGHT_PINS definition to initialize sw_pwm_ports/pins

/* Static global variables for Software PWM */
// These are the actual definitions for the SW PWM system.
// led_control.c's driveLED will need a way to set sw_pwm_duty_cycles.
// This will be done by adding a setter function in this file,
// and removing the extern declarations from led_control.c.

static const uint8_t sw_pwm_pin_indices[NUM_SW_PWM_CHANNELS] = {1, 2, 4, 5, 6, 7}; // From original main.c
static GPIO_TypeDef* sw_pwm_ports[NUM_SW_PWM_CHANNELS];
static uint16_t      sw_pwm_gpio_pins[NUM_SW_PWM_CHANNELS];
/*volatile*/ static uint8_t sw_pwm_duty_cycles[NUM_SW_PWM_CHANNELS] = {0}; // Made static, not extern. Needs setter.
/*volatile*/ static uint8_t sw_pwm_counter = 0; // Made static.

// Setter function for duty cycles, to be called by driveLED
void set_sw_pwm_channel_duty(uint8_t sw_channel_idx, uint8_t duty_0_to_resolution) {
    if (sw_channel_idx < NUM_SW_PWM_CHANNELS) {
        if (duty_0_to_resolution > SW_PWM_RESOLUTION) {
            sw_pwm_duty_cycles[sw_channel_idx] = SW_PWM_RESOLUTION;
        } else {
            sw_pwm_duty_cycles[sw_channel_idx] = duty_0_to_resolution;
        }
    }
}

// Getter function for sw_pwm_pin_indices, to be used by driveLED
// This is not ideal, driveLED should ideally not need to know these indices.
// A better approach would be for driveLED to call a function like:
// set_sw_pwm_for_led_idx(uint8_t led_idx, uint8_t val_0_255)
// For now, to match original structure closely first:
const uint8_t* get_sw_pwm_pin_indices(void) {
    return sw_pwm_pin_indices;
}


void init_software_pwm(void) {
    // Initialize port and pin arrays for SW PWM channels
    // This uses LIGHT_PINS from led_control.h/c
    for (int i = 0; i < NUM_SW_PWM_CHANNELS; ++i) {
        uint8_t light_pin_idx = sw_pwm_pin_indices[i]; // This is the index within LIGHT_PINS array
        if (light_pin_idx < LIGHT_PIN_COUNT) { // LIGHT_PIN_COUNT from led_control.h
            sw_pwm_ports[i] = LIGHT_PINS[light_pin_idx].port;
            sw_pwm_gpio_pins[i] = LIGHT_PINS[light_pin_idx].pin;
        }
        // Else: configuration error, pin index out of bounds
    }
    // Initialize duty cycles to 0
    for (int i = 0; i < NUM_SW_PWM_CHANNELS; ++i) {
        sw_pwm_duty_cycles[i] = 0;
    }
    sw_pwm_counter = 0;
}

void update_software_pwm(void) {
    sw_pwm_counter++;
    if (sw_pwm_counter >= SW_PWM_RESOLUTION) {
        sw_pwm_counter = 0;
    }

    for (int i = 0; i < NUM_SW_PWM_CHANNELS; ++i) {
        // Ensure sw_pwm_ports[i] and sw_pwm_gpio_pins[i] are valid before dereferencing
        if (sw_pwm_ports[i] == NULL) continue; // Basic safety check

        uint8_t current_duty = sw_pwm_duty_cycles[i];
        // Duty cycle is already scaled to SW_PWM_RESOLUTION by the setter
        // if (current_duty > SW_PWM_RESOLUTION) { // This check should be in the setter
        //      current_duty = SW_PWM_RESOLUTION;
        // }

        if (sw_pwm_counter < current_duty) {
            HAL_GPIO_WritePin(sw_pwm_ports[i], sw_pwm_gpio_pins[i], GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(sw_pwm_ports[i], sw_pwm_gpio_pins[i], GPIO_PIN_RESET);
        }
    }
}
