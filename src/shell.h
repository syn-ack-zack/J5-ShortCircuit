#ifndef SHELL_H
#define SHELL_H

#include "stm32l0xx_hal.h" // For HAL_UART_Transmit if used directly, or for types
#include <stdbool.h>
#include <stddef.h> // For size_t

/* Extern Constant Strings (defined in shell.c) */
extern const char* FW_VERSION_PGM; // Changed from char[] to char*
extern const char* const BANNER_STRINGS[]; // This one is already a pointer to const char pointers, which is fine
extern const char* BATT_PLACEHOLDER_STR; // Changed from char[] to char*
extern const char* EFFECT_PLACEHOLDER_STR; // Changed from char[] to char*

/* Function Prototypes */
void print_banner_shell(void); // Adapted from original printBanner
void cmd_parser_shell(char* cmd); // Adapted from original cmdParser
void shell_process_char(uint8_t rx_char, UART_HandleTypeDef* huart_shell); // New function to handle input
void init_shell(void); // For any one-time initializations

/* Utility functions (can be static in shell.c if not needed elsewhere) */
/* If they are needed by other modules, they should be in utils.h */
/* For now, assume they are primarily for shell.c internal use or can be duplicated if small */
// char* trim(char *str); // Will be in shell.c or utils.c
// int simple_strcasecmp(const char *s1, const char *s2); // Will be in shell.c or utils.c
// int simple_strncasecmp(const char *s1, const char *s2, size_t n); // Will be in shell.c or utils.c

#endif // SHELL_H
