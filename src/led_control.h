#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include "stm32l0xx_hal.h"
#include <stdbool.h>

/* Type Definitions */
typedef struct {
    GPIO_TypeDef* port;
    uint16_t      pin;
} LED_Pin_t;

typedef enum {
  EFFECT_OFF,        // 0
  EFFECT_STRIKE,     // 1
  EFFECT_CRACKLE,    // 2
  EFFECT_BREATHE,    // 3
  EFFECT_ALL_ON,     // 4 (was EFFECT_SPARKLE, behavior: all LEDs solid ON)
  EFFECT_SCANNER,    // 5 (New: K.I.T.T. style scanner)
  EFFECT_CONVERGE_DIVERGE // 6 (New: LEDs sweep from ends, meet, then sweep out)
} AppEffect_t;

/* Extern Global Variables (defined in main.c or led_control.c) */
extern volatile AppEffect_t effect;
extern volatile bool burstActive; // Used by STRIKE effect
extern LED_Pin_t LIGHT_PINS[]; // Definition will be in led_control.c
extern const uint8_t custom_marquee_sequence[]; // Definition will be in led_control.c

/* Constants */
#define LIGHT_PIN_COUNT 8 // Ensure this matches the array definition

// Eye LED specific constants (can be moved to led_control.c if only used there)
#define EYE_LED_PORT            GPIOA
#define EYE_LED_PIN             GPIO_PIN_0
#define EYE_SOLID_ON_BRIGHTNESS 200
#define EYE_PULSE_DIM_BRIGHTNESS 50
#define EYE_PULSE_PEAK_BRIGHTNESS 255
#define EYE_PULSE_INTERVAL_MS 5000UL
#define EYE_PULSE_DIM_DURATION_MS 100
#define EYE_PULSE_PEAK_DURATION_MS 150
#define EYE_PULSE_RETURN_DURATION_MS 100

// Strike Effect Timing Constants (can be moved to led_control.c)
#define STRIKE_RESTART_DELAY_MS 10000UL
#define STRIKE_PHASE1_DURATION 5000UL
#define STRIKE_PHASE1_SPARKLE_START_OFFSET 2500UL
#define STRIKE_PHASE1_SPARKLE_RAMP_DURATION (STRIKE_PHASE1_DURATION - STRIKE_PHASE1_SPARKLE_START_OFFSET)
#define STRIKE_PHASE2_FADE_DURATION 3500UL

/* Function Prototypes */
void driveLED(uint8_t led_idx, uint8_t val);
void clearAllLEDs(void);
const char* getEffectName(AppEffect_t current_effect_val);
void update_led_visuals(uint32_t now); // Main function to update current effect
void init_led_effects(void); // Optional: For one-time initializations if needed

// Functions to manage effect state changes (called by shell or touch input)
void cycle_effect(void);
void set_effect(AppEffect_t new_effect);


#endif // LED_CONTROL_H
