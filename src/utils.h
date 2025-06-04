#ifndef UTILS_H
#define UTILS_H

#include "stm32l0xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h> // For size_t

/* Type Definitions */
typedef enum {
    MORSE_TARGET_ALL_LEDS,
    MORSE_TARGET_EYES_ONLY
} MorseTarget_t;

/* Constants */
// Morse Code Timing Constants
#define DOT_MS        240
#define DASH_MS       (DOT_MS * 3)
#define SYMBOL_GAP_MS (DOT_MS + DOT_MS / 2)
#define LETTER_GAP_MS (DOT_MS * 5)
#define WORD_GAP_MS   (DOT_MS * 7)

// Capacitive Touch Pad
#define CAP_PAD_PORT            GPIOB
#define CAP_PAD_PIN             GPIO_PIN_7
#define TOUCH_MODE_CHANGE_COOLDOWN_MS 500 // Cooldown for touch input

/* Extern Constant Data (defined in utils.c) */
extern const char* MORSE_TABLE_C[36];

/* Function Prototypes */
void flash_morse_code(const char* msg, MorseTarget_t target);
uint16_t read_vdd_mv(void);
uint8_t get_battery_pct(uint16_t mv);
bool is_capacitive_touched(void);

// String utilities
char* trim(char *str);
int simple_strcasecmp(const char *s1, const char *s2);
int simple_strncasecmp(const char *s1, const char *s2, size_t n);

#endif // UTILS_H
