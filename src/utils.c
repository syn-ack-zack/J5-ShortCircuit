#include "utils.h"
#include "led_control.h" // For driveLED, clearAllLEDs, AppEffect_t, EYE_SOLID_ON_BRIGHTNESS, effect, burstActive
#include "hal_init.h"    // For htim2 (used by flash_morse_code for eye LED)
#include <ctype.h>       // For tolower, isspace
#include <string.h>      // For strlen
#include <stdio.h>       // For sprintf in batteryPct (if it were more complex)

/* Extern global variables from other modules needed by utils */
// These are defined in main.c and extern'd in their respective .h files
extern volatile AppEffect_t effect; // From led_control.h
extern volatile bool burstActive; // From led_control.h
extern ADC_HandleTypeDef hadc;    // From hal_init.h
extern TIM_HandleTypeDef htim2;   // From hal_init.h (for eye LED in flashMorse)


/* Morse Code Table Definition */
const char* MORSE_TABLE_C[36] = {
  ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
  "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
  "..-", "...-", ".--", "-..-", "-.--", "--..",
  "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...",
  "---..", "----."
};

void flash_morse_code(const char* msg, MorseTarget_t target) {
  // Save current LED state to restore later
  AppEffect_t oldEffect = effect;
  bool oldBurstState = burstActive;
  // It might also be necessary to save current PWM values if effects don't fully reset them.
  // For simplicity, assuming effects re-initialize their LEDs.

  // Temporarily take control of LEDs
  effect = EFFECT_OFF; // Stop other bling effects from main loop
  burstActive = false; // Stop strike effect

  // Turn off LEDs first
  if (target == MORSE_TARGET_EYES_ONLY) {
    for(uint8_t i=0; i < LIGHT_PIN_COUNT; ++i) { // Turn off all light bar LEDs
        driveLED(i, 0); // driveLED is in led_control.c
    }
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0); // Eye LED off (TIM2_CH1)
  } else { // MORSE_TARGET_ALL_LEDS
    clearAllLEDs(); // clearAllLEDs is in led_control.c
  }
  HAL_Delay(1000); // Wait 1 second with LEDs off

  while (*msg) {
    char c = *msg++;
    if (c >= 'a' && c <= 'z') c = toupper(c);

    if (c == ' ') {
      HAL_Delay(WORD_GAP_MS - LETTER_GAP_MS); // Account for upcoming letter gap
      continue;
    }

    uint8_t morse_idx;
    if (c >= 'A' && c <= 'Z') morse_idx = c - 'A';
    else if (c >= '0' && c <= '9') morse_idx = c - '0' + 26;
    else continue; // Skip unknown characters

    const char* pattern = MORSE_TABLE_C[morse_idx];
    while (*pattern) {
      uint16_t dur = (*pattern == '.') ? DOT_MS : DASH_MS;

      // --- Element ON ---
      if (target == MORSE_TARGET_EYES_ONLY) {
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS); // Eye ON
      } else { // MORSE_TARGET_ALL_LEDS
        for(uint8_t i=0; i < LIGHT_PIN_COUNT; ++i) driveLED(i, 255);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS); // Eye also blinks
      }
      HAL_Delay(dur);

      // --- Element OFF ---
      if (target == MORSE_TARGET_EYES_ONLY) {
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0); // Eye OFF
      } else { // MORSE_TARGET_ALL_LEDS
        for(uint8_t i=0; i < LIGHT_PIN_COUNT; ++i) driveLED(i, 0);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0); // Eye also off
      }

      pattern++;
      if (*pattern) HAL_Delay(SYMBOL_GAP_MS);
    }
    HAL_Delay(LETTER_GAP_MS);
  }

  // Restore states
  effect = oldEffect;
  burstActive = oldBurstState;

  // The main loop's update_led_visuals() should now correctly restore the visual state.
  // If the oldEffect was EFFECT_OFF, its specific eye handling (pulse or solid)
  // will be re-established by update_led_visuals().
  // A final clearAllLEDs() might be too abrupt.
  // If oldEffect was EFFECT_OFF, ensure eye is handled correctly by its pulse or solid state.
   if (effect == EFFECT_OFF) {
      // Re-initialize eye state for EFFECT_OFF as main loop might not catch it immediately
      // This logic is now inside update_led_visuals, so calling it once might be enough,
      // or let the main loop handle it. For safety, can re-init eye pulse state vars if they were static here.
      // The eye pulse state variables are static in led_control.c, so they retain their state.
      // The main loop calling update_led_visuals will resume the EFFECT_OFF eye logic.
      // No specific action needed here beyond restoring `effect`.
  }
}

uint16_t read_vdd_mv(void) {
  uint32_t raw_adc_val;
  // VREFINT_CAL_ADDR is defined in STM32 HAL (e.g. stm32l0xx_hal_adc_ex.h)
  // For STM32L0 series, it's typically *(uint16_t *)0x1FF80078 for Vdda=3.0V
  // Or *(uint16_t *)0x1FF800F8 for Vdda=1.8V
  // Assuming Vdda is 3.0V range for this calculation.
  uint16_t vrefint_cal_val = *(__IO uint16_t *)((uint32_t)0x1FF80078UL); // VREFINT_CAL_ADDR for 3V

  if (HAL_ADC_Start(&hadc) != HAL_OK) return 1; // Error
  if (HAL_ADC_PollForConversion(&hadc, HAL_MAX_DELAY) == HAL_OK) {
    raw_adc_val = HAL_ADC_GetValue(&hadc);
  } else {
    HAL_ADC_Stop(&hadc);
    return 2; // Error
  }
  HAL_ADC_Stop(&hadc);

  if (raw_adc_val == 0) return 3; // Error, division by zero

  // Vdda = 3.0V * VREFINT_CAL / VREFINT_DATA
  // The STM32L0 HAL might provide VREFINT_CAL_VREF for the voltage at which VREFINT_CAL was measured (e.g. 3000mV)
  // uint32_t vdda = (3000UL * (uint32_t)vrefint_cal_val) / raw_adc_val;
  // For STM32L0, VREFINT_CAL is the raw ADC data measured with Vdda=3.0V.
  // So, Vdda (mV) = 3000 * VREFINT_CAL_VALUE / ADC_RAW_VALUE_OF_VREFINT
  return (uint16_t)((3000UL * (uint32_t)vrefint_cal_val) / raw_adc_val);
}

uint8_t get_battery_pct(uint16_t mv) {
    // Simple linear mapping, adjust thresholds as needed for the specific battery
    if (mv < 2000) return 0;   // Below 2.0V = 0%
    if (mv >= 3000) return 100; // 3.0V and above = 100% (assuming LiPo max is higher, but this is for Vdd)
                                // This is likely Vdd measurement, not direct battery.
                                // If it's a 3.3V system, 3.0V might be ~70-80% for a LiPo.
                                // Original mapping: (mv - 2000) / 10 for range 2.0V to 3.0V
    return (uint8_t)((mv - 2000) / 10);
}

bool is_capacitive_touched(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Configure pin as output Push-Pull
    GPIO_InitStruct.Pin = CAP_PAD_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(CAP_PAD_PORT, &GPIO_InitStruct);

    // Drive pin high (charge the pad)
    HAL_GPIO_WritePin(CAP_PAD_PORT, CAP_PAD_PIN, GPIO_PIN_SET);
    // Small delay might be needed for charging, but original didn't have one.

    // Configure pin as input
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    // GPIO_InitStruct.Pull = GPIO_NOPULL; // Or GPIO_PULLDOWN if expecting it to discharge faster
    HAL_GPIO_Init(CAP_PAD_PORT, &GPIO_InitStruct);

    // Measure discharge time (count how long it stays high)
    uint32_t count = 0;
    // Max count to prevent infinite loop if something is wrong
    // Original used 10000, threshold 2000.
    while (HAL_GPIO_ReadPin(CAP_PAD_PORT, CAP_PAD_PIN) == GPIO_PIN_SET && count < 10000) {
        count++;
    }
    return (count < 2000);
}

// String utilities
char* trim(char *str) {
    char *end;
    // Trim leading space
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0)  // All spaces?
        return str;
    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    // Write new null terminator character
    *(end+1) = '\0';
    return str;
}

int simple_strcasecmp(const char *s1, const char *s2) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    int result;
    if (p1 == p2) return 0;
    while ((result = tolower(*p1) - tolower(*p2++)) == 0) {
        if (*p1++ == '\0') break;
    }
    return result;
}

int simple_strncasecmp(const char *s1, const char *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    int result = 0;
    if (p1 == p2 || n == 0) return 0;
    while (n-- > 0 && (result = tolower(*p1) - tolower(*p2++)) == 0) {
        if (*p1++ == '\0') break;
        if (n == 0) break; // Check n before incrementing p1 inside loop condition
    }
    return result;
}
