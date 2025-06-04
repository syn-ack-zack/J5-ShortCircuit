#include "led_control.h"
#include "sw_pwm.h" // For sw_pwm_duty_cycles if driveLED directly manipulates it
#include "hal_init.h" // For TIM handles like htim2
#include <string.h>   // For strcmp in getEffectName (though not strictly needed if only switch)
#include <stdio.h>    // For snprintf if any debug messages were to be added
#include <stdlib.h>   // For rand()

/* Global variables related to LED effects (defined here) */
// AppEffect_t effect is defined in main.c and extern in led_control.h
// bool burstActive is defined in main.c and extern in led_control.h

/* Static variables for LED effects states */
// Strike Effect
static uint8_t  strike_phase = 0;
static uint32_t strike_phaseStartTime = 0;
static uint32_t strike_lastStepTime = 0;
static uint32_t strike_lastSparkleTime = 0;
static uint32_t lastBurstTriggerTime = 0; // Renamed from main.c's lastBurstTriggerTime for clarity if needed
static uint8_t intro_sub_phase = 0;
static uint8_t intro_leader_idx = 0;
static const uint8_t intro_leader_sequence[] = {4, 3, 2, 1, 0, 6, 5, 7};
#define INTRO_LEADER_SEQUENCE_LENGTH (sizeof(intro_leader_sequence)/sizeof(intro_leader_sequence[0]))

// Eye Pulse for EFFECT_OFF
static uint32_t last_eye_pulse_trigger_time = 0;
static uint8_t  eye_pulse_sub_phase = 0;
static uint32_t eye_pulse_sub_phase_start_time = 0;

// Generic Eye Animation (for non-strike/non-off modes)
static uint32_t eyeLast_generic = 0;
static int16_t  eyeLvl_generic  = 0;
static int8_t   eyeDir_generic  = 4;


/* LED Pin Definitions */
LED_Pin_t LIGHT_PINS[LIGHT_PIN_COUNT] = {
    {GPIOA, GPIO_PIN_1}, {GPIOA, GPIO_PIN_8}, {GPIOB, GPIO_PIN_1}, {GPIOA, GPIO_PIN_6},
    {GPIOB, GPIO_PIN_3}, {GPIOB, GPIO_PIN_6}, {GPIOA, GPIO_PIN_5}, {GPIOB, GPIO_PIN_0}
};

const uint8_t custom_marquee_sequence[LIGHT_PIN_COUNT] = {4, 3, 2, 1, 0, 6, 5, 7}; // Matches original
#define CUSTOM_MARQUEE_SEQUENCE_LENGTH (sizeof(custom_marquee_sequence)/sizeof(custom_marquee_sequence[0]))


// Software PWM related variables needed by driveLED if it directly manipulates them
// These are defined in sw_pwm.c and should be accessed via a function if possible,
// but original driveLED modified sw_pwm_duty_cycles directly.
// For now, assume sw_pwm.h provides extern declaration if needed, or sw_pwm.c provides a setter.
// Based on sw_pwm.h, these are static in sw_pwm.c.
// driveLED will need to call a setter in sw_pwm.c or sw_pwm_duty_cycles needs to be extern.
// For now, I'll assume sw_pwm.c will expose sw_pwm_duty_cycles or a setter.
// Let's assume sw_pwm.h will declare `extern volatile uint8_t sw_pwm_duty_cycles[NUM_SW_PWM_CHANNELS];`
// and sw_pwm.c will define it.
// Re-checking sw_pwm.h: it comments out extern declarations.
// This means driveLED needs to be refactored or sw_pwm needs to expose setters.
// For now, I will assume `sw_pwm_duty_cycles` is made extern in `sw_pwm.h` and defined in `sw_pwm.c`.
// Note: sw_pwm_pin_indices and sw_pwm_duty_cycles are now accessed via functions
// from sw_pwm.c


void driveLED(uint8_t led_idx, uint8_t val) {
    if (led_idx >= LIGHT_PIN_COUNT) return;

    const uint8_t* current_sw_pwm_pin_indices = get_sw_pwm_pin_indices();
    bool is_sw_pwm = false;
    for (int i = 0; i < NUM_SW_PWM_CHANNELS; ++i) {
        if (led_idx == current_sw_pwm_pin_indices[i]) {
            uint8_t scaled_duty = (uint8_t)(((uint32_t)val * SW_PWM_RESOLUTION) / 255);
            set_sw_pwm_channel_duty(i, scaled_duty);
            is_sw_pwm = true;
            break;
        }
    }

    if (is_sw_pwm) {
        return;
    }

    // Hardware PWM channels
    uint8_t pwm_value = val;
    switch(led_idx) {
        case 0: // PA1 (LIGHT_PINS[0]) -> TIM2_CH2 (as per original main.c, assuming PA1 is TIM2_CH2)
                // Original driveLED case 0 was TIM2_CH2. LIGHT_PINS[0] is PA1.
                // HAL_TIM_PWM_MspInit configures PA0 as TIM2_CH1 and PA1 as TIM2_CH2.
                // So led_idx 0 (LIGHT_PINS[0] = PA1) should control TIM2_CH2.
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pwm_value);
            break;
        case 3: // PA6 (LIGHT_PINS[3]) -> TIM22_CH1 (as per original main.c, assuming PA6 is TIM22_CH1)
                // Original driveLED case 3 was TIM22_CH1. LIGHT_PINS[3] is PA6.
                // HAL_TIM_PWM_MspInit configures PA6 as TIM22_CH1.
            __HAL_TIM_SET_COMPARE(&htim22, TIM_CHANNEL_1, pwm_value);
            break;
        // Other hardware PWM pins if any would be here.
        // The eye LED (PA0) is TIM2_CH1, handled separately or via its own index if added to LIGHT_PINS logic.
        // Original code handles eye via __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ...);
    }
}

void clearAllLEDs(void) {
    for (uint8_t i = 0; i < LIGHT_PIN_COUNT; ++i) {
        driveLED(i, 0);
    }
    // Also explicitly turn off the Eye LED (PA0, TIM2_CH1)
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
}

const char* getEffectName(AppEffect_t current_effect_val) {
    switch (current_effect_val) {
        case EFFECT_OFF: return "IDLE";
        case EFFECT_STRIKE: return "STRIKE";
        case EFFECT_CRACKLE: return "CRACKLE";
        case EFFECT_BREATHE: return "BREATHE";
        case EFFECT_ALL_ON: return "ALL ON";
        case EFFECT_SCANNER: return "SCANNER";
        case EFFECT_CONVERGE_DIVERGE: return "CONV";
        default: return "UNKNOWN";
    }
}

void init_led_effects(void) {
    // Initialize static variables for effects if needed, e.g., random seeds, initial states.
    // Most are initialized at declaration or when an effect starts.
    // srand(HAL_GetTick()); // srand is called in main.c
}


// This function combines the logic from the main loop's effect handling
// and the original updateBlingEffects()
void update_led_visuals(uint32_t now) {
    // Generic eye animation for non-strike/non-off modes (if not handled by specific effect)
    if (effect != EFFECT_OFF && effect != EFFECT_STRIKE) {
      if (now - eyeLast_generic >= 20) {
        eyeLast_generic = now;
        eyeLvl_generic += eyeDir_generic;
        if (eyeLvl_generic >= 255) { eyeLvl_generic = 255; eyeDir_generic = -eyeDir_generic; }
        if (eyeLvl_generic <= 0)   { eyeLvl_generic = 0;   eyeDir_generic = -eyeDir_generic; }
        uint8_t pwm_val = (uint8_t)eyeLvl_generic;
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pwm_val); // Eye LED on TIM2_CH1
      }
    }
    // Note: EFFECT_STRIKE controls its eye directly.
    // EFFECT_OFF has its own eye pulse logic handled in its main effect block.


    if (effect == EFFECT_STRIKE) {
        const uint32_t SPARKLE_INTERVAL_MS = 30;
        const uint8_t  SPARKLE_MAX_DENSITY_VAL = 4;

        if (!burstActive && (now - lastBurstTriggerTime >= STRIKE_RESTART_DELAY_MS)) {
            burstActive = true;
            strike_phase = 0;
            strike_phaseStartTime = now;
            strike_lastStepTime = now;
            strike_lastSparkleTime = now;
            intro_sub_phase = 0;
            intro_leader_idx = 0;
            clearAllLEDs();
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS);
        }

        if (burstActive) {
            uint32_t phaseElapsedTime = now - strike_phaseStartTime;
            switch (strike_phase) {
                case 0: // Intro sequence
                {
                    const uint32_t INTRO_LEADER_LED_DURATION_MS = 40;
                    const uint8_t  INTRO_LEADER_BRIGHTNESS = 255;
                    const uint32_t INTRO_DELAY_AFTER_LEADER_MS = 50;
                    const uint32_t NEW_INTRO_FLASH_ON_MS = 100;
                    const uint32_t NEW_INTRO_FLASH_OFF_MS = 100;
                    const uint8_t  INTRO_FLASH_BRIGHTNESS = 255;
                    const uint32_t HOLD_ALL_ON_DURATION_MS = 500;

                    if (intro_sub_phase == 0) { // Leader animation
                        if (now - strike_lastStepTime >= INTRO_LEADER_LED_DURATION_MS) {
                            strike_lastStepTime = now;
                            clearAllLEDs();
                            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS);
                            if (intro_leader_idx < INTRO_LEADER_SEQUENCE_LENGTH) {
                                driveLED(intro_leader_sequence[intro_leader_idx], INTRO_LEADER_BRIGHTNESS);
                                intro_leader_idx++;
                            }
                            if (intro_leader_idx >= INTRO_LEADER_SEQUENCE_LENGTH) {
                                intro_sub_phase = 6; // Transition to delay
                                intro_leader_idx = 0; // Reset for potential reuse
                                clearAllLEDs();
                                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS);
                                strike_lastStepTime = now; // Reset timer for delay
                            }
                        }
                    } else if (intro_sub_phase == 6) { // Delay after leader
                        if (now - strike_lastStepTime >= INTRO_DELAY_AFTER_LEADER_MS) {
                            intro_sub_phase = 1; // Transition to first flash
                        }
                    } else if (intro_sub_phase == 1) { // First flash ON
                        for (uint8_t i = 0; i < INTRO_LEADER_SEQUENCE_LENGTH; ++i) {
                            driveLED(intro_leader_sequence[i], INTRO_FLASH_BRIGHTNESS);
                        }
                        intro_sub_phase = 2;
                        strike_lastStepTime = now;
                    } else if (intro_sub_phase == 2) { // First flash ON duration
                        if (now - strike_lastStepTime >= NEW_INTRO_FLASH_ON_MS) {
                            clearAllLEDs();
                            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS);
                            intro_sub_phase = 3;
                            strike_lastStepTime = now;
                        }
                    } else if (intro_sub_phase == 3) { // First flash OFF duration
                        if (now - strike_lastStepTime >= NEW_INTRO_FLASH_OFF_MS) {
                            intro_sub_phase = 4; // Transition to second flash
                        }
                    } else if (intro_sub_phase == 4) { // Second flash ON
                         for (uint8_t i = 0; i < INTRO_LEADER_SEQUENCE_LENGTH; ++i) {
                            driveLED(intro_leader_sequence[i], INTRO_FLASH_BRIGHTNESS);
                        }
                        intro_sub_phase = 5;
                        strike_lastStepTime = now;
                    } else if (intro_sub_phase == 5) { // Second flash ON duration
                        if (now - strike_lastStepTime >= NEW_INTRO_FLASH_ON_MS) {
                            // No clear here, hold them on
                            intro_sub_phase = 7; // Transition to hold
                            strike_lastStepTime = now;
                        }
                    } else if (intro_sub_phase == 7) { // Hold all ON duration
                        if (now - strike_lastStepTime >= HOLD_ALL_ON_DURATION_MS) {
                            strike_phase = 1; // Transition to main strike (marquee)
                            strike_phaseStartTime = now; // Reset phase timer
                            strike_lastStepTime = now;
                            strike_lastSparkleTime = now;
                            // strike_marqueePos = 0; // marqueePos is static in main.c, will be moved here
                        }
                    }
                    break;
                }
                case 1: // Main strike phase (marquee + increasing sparkle)
                {
                    static int8_t strike_marqueePos = 0; // Now static to this function/file
                    const uint8_t strike_marqueeLength = 5;
                    const uint32_t MARQUEE_STEP_MS = 110;
                    const uint8_t MARQUEE_BRIGHTNESS = 255;

                    if (now - strike_lastStepTime >= MARQUEE_STEP_MS) {
                        strike_lastStepTime = now;
                        clearAllLEDs();
                        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS);

                        if (CUSTOM_MARQUEE_SEQUENCE_LENGTH > 0 && strike_marqueeLength == 5) { // Original condition
                            for (int i = 0; i < strike_marqueeLength; ++i) {
                                int offset = i - (strike_marqueeLength / 2);
                                int sequence_idx_to_light = (strike_marqueePos + offset + CUSTOM_MARQUEE_SEQUENCE_LENGTH) % CUSTOM_MARQUEE_SEQUENCE_LENGTH;
                                uint8_t led_pin_idx_in_LIGHT_PINS = custom_marquee_sequence[sequence_idx_to_light];
                                driveLED(led_pin_idx_in_LIGHT_PINS, MARQUEE_BRIGHTNESS);
                            }
                        }
                        strike_marqueePos = (strike_marqueePos + 1) % CUSTOM_MARQUEE_SEQUENCE_LENGTH;
                    }

                    // Sparkle logic (increasing density)
                    if (phaseElapsedTime >= STRIKE_PHASE1_SPARKLE_START_OFFSET) {
                        if (now - strike_lastSparkleTime >= SPARKLE_INTERVAL_MS) {
                            strike_lastSparkleTime = now;
                            uint8_t numSparklesThisHit = 0;
                            if (STRIKE_PHASE1_SPARKLE_RAMP_DURATION > 0) {
                                uint32_t timeIntoSparkleRamp = phaseElapsedTime - STRIKE_PHASE1_SPARKLE_START_OFFSET;
                                numSparklesThisHit = (SPARKLE_MAX_DENSITY_VAL * timeIntoSparkleRamp) / STRIKE_PHASE1_SPARKLE_RAMP_DURATION;
                            }
                            if (numSparklesThisHit > SPARKLE_MAX_DENSITY_VAL) numSparklesThisHit = SPARKLE_MAX_DENSITY_VAL;

                            if (numSparklesThisHit > 0) {
                                for (uint8_t i = 0; i < numSparklesThisHit; ++i) {
                                    driveLED(rand() % LIGHT_PIN_COUNT, 255);
                                }
                            }
                        }
                    }

                    if (phaseElapsedTime >= STRIKE_PHASE1_DURATION) {
                        strike_phase = 2; // Transition to fade out
                        strike_phaseStartTime = now; // Reset phase timer
                        strike_lastSparkleTime = now; // Reset sparkle timer for fade phase
                    }
                    break;
                }
                case 2: // Fade out phase (decreasing sparkle)
                {
                    if (now - strike_lastSparkleTime >= SPARKLE_INTERVAL_MS) {
                        strike_lastSparkleTime = now;
                        clearAllLEDs();
                        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS);

                        uint8_t numSparklesThisHit = 0;
                        if (phaseElapsedTime < STRIKE_PHASE2_FADE_DURATION) {
                            uint32_t timeIntoFade = phaseElapsedTime;
                            numSparklesThisHit = (SPARKLE_MAX_DENSITY_VAL * (STRIKE_PHASE2_FADE_DURATION - timeIntoFade)) / STRIKE_PHASE2_FADE_DURATION;
                            if (numSparklesThisHit > SPARKLE_MAX_DENSITY_VAL) numSparklesThisHit = SPARKLE_MAX_DENSITY_VAL;
                        } else {
                            numSparklesThisHit = 0;
                        }

                        if (numSparklesThisHit > 0) {
                            for (uint8_t i = 0; i < numSparklesThisHit; ++i) {
                                driveLED(rand() % LIGHT_PIN_COUNT, 255);
                            }
                        }
                    }

                    if (phaseElapsedTime >= STRIKE_PHASE2_FADE_DURATION) {
                        if (now - strike_lastSparkleTime > SPARKLE_INTERVAL_MS) { // Ensure one last clear after sparkles stop
                           clearAllLEDs();
                           __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS); // Keep eye on
                        }
                        burstActive = false; // End of burst
                        lastBurstTriggerTime = now; // For STRIKE_RESTART_DELAY_MS
                    }
                    break;
                }
            }
        }
    }
    else if (effect == EFFECT_OFF) {
        if (burstActive) burstActive = false; // Ensure strike is off

        // Turn off all light bar LEDs
        for(uint8_t i=0; i < LIGHT_PIN_COUNT; ++i) {
            // Assuming EYE_LED is not part of LIGHT_PINS or handled by its index if it is.
            // Original code had a check: if(LIGHT_PINS[i].port != EYE_LED_PORT || LIGHT_PINS[i].pin != EYE_LED_PIN)
            // This implies eye LED might be in LIGHT_PINS. For simplicity, clearAllLEDs() handles this.
            // Here, we just ensure light bar is off. Eye is handled by pulse.
            driveLED(i,0);
        }

        // Eye pulse logic for EFFECT_OFF
        if (eye_pulse_sub_phase == 0) { // Solid ON, waiting for pulse interval
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS);
            if (now - last_eye_pulse_trigger_time >= EYE_PULSE_INTERVAL_MS) {
                eye_pulse_sub_phase = 1; // Start pulse: Dim
                eye_pulse_sub_phase_start_time = now;
                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_PULSE_DIM_BRIGHTNESS);
            }
        } else if (eye_pulse_sub_phase == 1) { // Dim phase
            if (now - eye_pulse_sub_phase_start_time >= EYE_PULSE_DIM_DURATION_MS) {
                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_PULSE_PEAK_BRIGHTNESS);
                eye_pulse_sub_phase = 2; // Peak phase
                eye_pulse_sub_phase_start_time = now;
            }
        } else if (eye_pulse_sub_phase == 2) { // Peak phase
            if (now - eye_pulse_sub_phase_start_time >= EYE_PULSE_PEAK_DURATION_MS) {
                 __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS); // Return to solid (dimmer than peak, brighter than dim)
                eye_pulse_sub_phase = 3; // Return phase
                eye_pulse_sub_phase_start_time = now;
            }
        } else if (eye_pulse_sub_phase == 3) { // Return to solid phase
             if (now - eye_pulse_sub_phase_start_time >= EYE_PULSE_RETURN_DURATION_MS) {
                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS); // Ensure solid
                eye_pulse_sub_phase = 0; // Wait for next interval
                last_eye_pulse_trigger_time = now; // Reset interval timer
            }
        }
    }
    else { // Other Bling Effects (formerly updateBlingEffects())
        if (burstActive) burstActive = false; // Ensure strike is off

        static uint32_t t0_effects_bling = 0; // Renamed from original updateBlingEffects t0_effects
        switch (effect) {
            case EFFECT_CRACKLE: {
              static uint32_t t0_crackle = 0;
              __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS);

              if (now - t0_crackle > 20) { // Original interval
                t0_crackle = now;
                for (uint8_t i = 0; i < LIGHT_PIN_COUNT; ++i) { driveLED(i, 0); } // Clear light bar
                for (uint8_t i = 0; i < 4; ++i) { // Sparkle 4 LEDs
                    driveLED(rand() % LIGHT_PIN_COUNT, 255);
                }
              }
              break;
            }
            case EFFECT_BREATHE: {
              static int16_t breathe_level = 0; static int8_t breathe_dir_fx = 5; // Original params
              if (now - t0_effects_bling > 15) { // Original interval
                t0_effects_bling = now; breathe_level += breathe_dir_fx;
                if (breathe_level >= 255) { breathe_level = 255; breathe_dir_fx = -breathe_dir_fx; }
                if (breathe_level <=   0) { breathe_level =   0; breathe_dir_fx = -breathe_dir_fx; }
                for (uint8_t p_idx = 0; p_idx < LIGHT_PIN_COUNT; ++p_idx) { driveLED(p_idx, (uint8_t)breathe_level); }
                // Eye is handled by generic eye animation
              } break;
            }
            case EFFECT_ALL_ON: {
              for (uint8_t p_idx = 0; p_idx < LIGHT_PIN_COUNT; ++p_idx) { driveLED(p_idx, 255); }
              __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 255); // Eye full on
              break;
            }
            case EFFECT_SCANNER: {
              static int8_t scanner_pos = 0; static int8_t scanner_dir = 1;
              static uint32_t t0_scanner = 0;
              const uint32_t SCANNER_SPEED_MS = 75; const uint8_t SCANNER_BRIGHTNESS = 255;
              const int8_t SCANNER_WIDTH = 2;

              __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS); // Eye solid on

              if (now - t0_scanner >= SCANNER_SPEED_MS) {
                t0_scanner = now;
                for (uint8_t i = 0; i < LIGHT_PIN_COUNT; ++i) { driveLED(i, 0); } // Clear light bar

                for (int8_t i = 0; i < SCANNER_WIDTH; ++i) {
                  int8_t current_led_idx_in_sequence = scanner_pos + i;
                  if (current_led_idx_in_sequence >= 0 && current_led_idx_in_sequence < CUSTOM_MARQUEE_SEQUENCE_LENGTH) {
                    driveLED(custom_marquee_sequence[current_led_idx_in_sequence], SCANNER_BRIGHTNESS);
                  }
                }
                scanner_pos += scanner_dir;
                if (scanner_dir == 1 && scanner_pos >= (CUSTOM_MARQUEE_SEQUENCE_LENGTH - SCANNER_WIDTH + 1)) {
                  scanner_pos = CUSTOM_MARQUEE_SEQUENCE_LENGTH - SCANNER_WIDTH; scanner_dir = -1;
                } else if (scanner_dir == -1 && scanner_pos < 0) {
                  scanner_pos = 0; scanner_dir = 1;
                }
              }
              break;
            }
            case EFFECT_CONVERGE_DIVERGE: {
                static enum { CONVERGE_INTERNAL_INIT, CONVERGE_INTERNAL_CONVERGING, CONVERGE_INTERNAL_PULSING } internal_phase_cd = CONVERGE_INTERNAL_INIT;
                static int8_t converge_p1, converge_p2;
                static int8_t pulse_current_led_idx_cd; static int16_t pulse_current_brightness_cd; static int8_t pulse_brightness_dir_cd;
                static uint32_t last_converge_step_time_cd = 0, last_pulse_sweep_time_cd = 0, last_pulse_glow_time_cd = 0;

                const uint32_t CONVERGE_ANIM_SPEED_MS = 180; const uint8_t CONVERGE_LED_BRIGHTNESS = 220;
                const uint32_t PULSE_ANIM_SWEEP_SPEED_MS = 50; const uint32_t PULSE_ANIM_GLOW_SPEED_MS = 50;
                const int16_t PULSE_ANIM_BRIGHTNESS_STEP = 1; const int16_t PULSE_ANIM_MAX_BRIGHTNESS = 255;
                const int16_t PULSE_ANIM_MIN_BRIGHTNESS = 20; const uint8_t PULSE_ANIM_BACKGROUND_BRIGHTNESS = 50;
                const uint8_t PULSE_GRADIENT_LENGTH = 3;
                const uint8_t PULSE_TAIL1_BRIGHTNESS_NUM = 3, PULSE_TAIL1_BRIGHTNESS_DEN = 5;
                const uint8_t PULSE_TAIL2_BRIGHTNESS_NUM = 3, PULSE_TAIL2_BRIGHTNESS_DEN = 10;
                const uint16_t TRANSITION_SCALE = 255;

                static AppEffect_t effect_last_tick_converge_cd = (AppEffect_t)-1;
                if (effect != effect_last_tick_converge_cd) {
                    if (effect == EFFECT_CONVERGE_DIVERGE) { internal_phase_cd = CONVERGE_INTERNAL_INIT; }
                    effect_last_tick_converge_cd = effect;
                }

                if (internal_phase_cd == CONVERGE_INTERNAL_INIT) {
                    converge_p1 = 0; converge_p2 = CUSTOM_MARQUEE_SEQUENCE_LENGTH - 1;
                    pulse_current_led_idx_cd = 0; pulse_current_brightness_cd = PULSE_ANIM_MIN_BRIGHTNESS; pulse_brightness_dir_cd = 1;
                    for (uint8_t i = 0; i < CUSTOM_MARQUEE_SEQUENCE_LENGTH; ++i) { driveLED(custom_marquee_sequence[i], PULSE_ANIM_BACKGROUND_BRIGHTNESS); }
                    internal_phase_cd = CONVERGE_INTERNAL_CONVERGING;
                    uint32_t current_time_init = HAL_GetTick();
                    last_converge_step_time_cd = current_time_init; last_pulse_sweep_time_cd = current_time_init; last_pulse_glow_time_cd = current_time_init;
                }

                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS);

                if (internal_phase_cd == CONVERGE_INTERNAL_CONVERGING) {
                    if (now - last_converge_step_time_cd >= CONVERGE_ANIM_SPEED_MS) {
                        last_converge_step_time_cd = now;
                        for (uint8_t i = 0; i < CUSTOM_MARQUEE_SEQUENCE_LENGTH; ++i) { driveLED(custom_marquee_sequence[i], PULSE_ANIM_BACKGROUND_BRIGHTNESS); }
                        if (converge_p1 >= 0 && converge_p1 < CUSTOM_MARQUEE_SEQUENCE_LENGTH) { driveLED(custom_marquee_sequence[converge_p1], CONVERGE_LED_BRIGHTNESS); }
                        if (converge_p2 >= 0 && converge_p2 < CUSTOM_MARQUEE_SEQUENCE_LENGTH && converge_p1 != converge_p2) { driveLED(custom_marquee_sequence[converge_p2], CONVERGE_LED_BRIGHTNESS); }
                        converge_p1++; converge_p2--;
                        if (converge_p1 >= converge_p2) {
                            internal_phase_cd = CONVERGE_INTERNAL_PULSING;
                            pulse_current_led_idx_cd = 0; pulse_current_brightness_cd = PULSE_ANIM_MIN_BRIGHTNESS; pulse_brightness_dir_cd = 1;
                            last_pulse_sweep_time_cd = now; last_pulse_glow_time_cd = now;
                        }
                    }
                } else if (internal_phase_cd == CONVERGE_INTERNAL_PULSING) {
                    // bool needs_redraw_cd = false; // Removed unused variable
                    if (now - last_pulse_glow_time_cd >= PULSE_ANIM_GLOW_SPEED_MS) {
                        last_pulse_glow_time_cd = now; // needs_redraw_cd was true here
                        pulse_current_brightness_cd += (pulse_brightness_dir_cd * PULSE_ANIM_BRIGHTNESS_STEP);
                        if (pulse_current_brightness_cd >= PULSE_ANIM_MAX_BRIGHTNESS) { pulse_current_brightness_cd = PULSE_ANIM_MAX_BRIGHTNESS; pulse_brightness_dir_cd = -1; }
                        else if (pulse_current_brightness_cd <= PULSE_ANIM_MIN_BRIGHTNESS) { pulse_current_brightness_cd = PULSE_ANIM_MIN_BRIGHTNESS; pulse_brightness_dir_cd = 1; }
                    }
                    uint16_t transition_progress_scaled_cd = 0;
                    if (now - last_pulse_sweep_time_cd >= PULSE_ANIM_SWEEP_SPEED_MS) {
                        // This means a full step in sweep has occurred or is due
                        transition_progress_scaled_cd = TRANSITION_SCALE; // At the target
                    } else {
                        transition_progress_scaled_cd = (uint16_t)(((now - last_pulse_sweep_time_cd) * TRANSITION_SCALE) / PULSE_ANIM_SWEEP_SPEED_MS);
                    }
                     if (transition_progress_scaled_cd > TRANSITION_SCALE) transition_progress_scaled_cd = TRANSITION_SCALE;


                    uint8_t led_brightness_map_cd[CUSTOM_MARQUEE_SEQUENCE_LENGTH];
                    for (uint8_t i = 0; i < CUSTOM_MARQUEE_SEQUENCE_LENGTH; ++i) { led_brightness_map_cd[i] = PULSE_ANIM_BACKGROUND_BRIGHTNESS; }
                    
                    uint8_t head_pulse_comp_brightness_cd = (pulse_current_brightness_cd < PULSE_ANIM_MIN_BRIGHTNESS) ? PULSE_ANIM_MIN_BRIGHTNESS : ((pulse_current_brightness_cd > PULSE_ANIM_MAX_BRIGHTNESS) ? PULSE_ANIM_MAX_BRIGHTNESS : (uint8_t)pulse_current_brightness_cd);

                    uint16_t outgoing_intensity_scaled_cd = TRANSITION_SCALE - transition_progress_scaled_cd;
                    for (uint8_t i = 0; i < PULSE_GRADIENT_LENGTH; ++i) {
                        int8_t led_map_idx = (pulse_current_led_idx_cd - i + CUSTOM_MARQUEE_SEQUENCE_LENGTH) % CUSTOM_MARQUEE_SEQUENCE_LENGTH;
                        uint32_t scaled_brightness = (uint32_t)head_pulse_comp_brightness_cd * outgoing_intensity_scaled_cd;
                        if (i == 1) { scaled_brightness = (scaled_brightness * PULSE_TAIL1_BRIGHTNESS_NUM) / PULSE_TAIL1_BRIGHTNESS_DEN; }
                        else if (i == 2) { scaled_brightness = (scaled_brightness * PULSE_TAIL2_BRIGHTNESS_NUM) / PULSE_TAIL2_BRIGHTNESS_DEN; }
                        uint8_t out_seg_brightness = (uint8_t)(scaled_brightness / TRANSITION_SCALE);
                        if (out_seg_brightness > led_brightness_map_cd[led_map_idx]) led_brightness_map_cd[led_map_idx] = out_seg_brightness;
                    }

                    uint16_t incoming_intensity_scaled_cd = transition_progress_scaled_cd;
                    int8_t next_led_base_idx_cd = (pulse_current_led_idx_cd + 1) % CUSTOM_MARQUEE_SEQUENCE_LENGTH;
                    for (uint8_t i = 0; i < PULSE_GRADIENT_LENGTH; ++i) {
                        int8_t led_map_idx = (next_led_base_idx_cd - i + CUSTOM_MARQUEE_SEQUENCE_LENGTH) % CUSTOM_MARQUEE_SEQUENCE_LENGTH;
                        uint32_t scaled_brightness = (uint32_t)head_pulse_comp_brightness_cd * incoming_intensity_scaled_cd;
                        if (i == 1) { scaled_brightness = (scaled_brightness * PULSE_TAIL1_BRIGHTNESS_NUM) / PULSE_TAIL1_BRIGHTNESS_DEN; }
                        else if (i == 2) { scaled_brightness = (scaled_brightness * PULSE_TAIL2_BRIGHTNESS_NUM) / PULSE_TAIL2_BRIGHTNESS_DEN; }
                        uint8_t in_seg_brightness = (uint8_t)(scaled_brightness / TRANSITION_SCALE);
                        if (in_seg_brightness > led_brightness_map_cd[led_map_idx]) led_brightness_map_cd[led_map_idx] = in_seg_brightness;
                    }

                    for (uint8_t i = 0; i < CUSTOM_MARQUEE_SEQUENCE_LENGTH; ++i) {
                        uint8_t final_brightness = led_brightness_map_cd[i];
                        if (final_brightness < PULSE_ANIM_BACKGROUND_BRIGHTNESS) final_brightness = PULSE_ANIM_BACKGROUND_BRIGHTNESS;
                        driveLED(custom_marquee_sequence[i], final_brightness);
                    }

                    if (now - last_pulse_sweep_time_cd >= PULSE_ANIM_SWEEP_SPEED_MS) { // Transition complete
                        last_pulse_sweep_time_cd = now;
                        pulse_current_led_idx_cd = (pulse_current_led_idx_cd + 1) % CUSTOM_MARQUEE_SEQUENCE_LENGTH;
                        // needs_redraw_cd = true; // This was the only other place it was set
                    }
                }
              break;
            }
            case EFFECT_STRIKE: // Should be handled by the primary if/else
            case EFFECT_OFF:    // Should be handled by the primary if/else
            default: break; // No other bling effects defined in original updateBlingEffects
        }
    }
}


// Placeholder for functions to change effect, called by shell or touch
// These will interact with global `effect` and `repair_status` (for saving)
// And potentially re-initialize effect-specific static variables.
// Actual implementation will need access to `repair_status` and `save_repair_status` from challenge module.
// For now, just prototypes.
void cycle_effect(void) {
    // This logic will be moved from main.c's touch handling section
    // Needs access to: effect, clearAllLEDs, AppEffect_t enum,
    // repair_status, save_repair_status (from challenge module),
    // strike effect re-init variables (burstActive, strike_phase, etc.)
    // eye pulse re-init variables (eye_pulse_sub_phase, etc.)
}

void set_effect(AppEffect_t new_effect) {
    // This logic will be moved from main.c's "bling" command
    // Needs access to: effect, clearAllLEDs, AppEffect_t enum,
    // repair_status, save_repair_status (from challenge module),
    // strike effect re-init variables, eye pulse re-init variables.
}
