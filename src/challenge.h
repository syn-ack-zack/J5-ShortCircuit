#ifndef CHALLENGE_H
#define CHALLENGE_H

#include "stm32l0xx_hal.h"
#include <stdbool.h>
#include <stdint.h> // For uint32_t, uint8_t

/* Type Definitions */
typedef struct {
    uint32_t magic_number;
    uint8_t challenge1_completed; // Comms module
    uint8_t challenge2_completed; // Power Core module
    uint8_t challenge3_completed; // Personality Matrix module
    uint8_t last_unlocked_effect; // Stores the AppEffect_t when unlocked
} Johnny5_RepairStatus_t;

/* Extern Global Variables (defined in main.c) */
extern Johnny5_RepairStatus_t repair_status;
extern volatile bool all_repairs_completed;
extern volatile bool diagnostic_stream_active;
extern int johnny5_chat_state; // Using int as in original, consider enum if states are well-defined
extern bool personality_matrix_fixed; // Tracks if challenge3 (personality_matrix) is completed

/* Constants */
#define J5_REPAIR_MAGIC_NUMBER 0xA5A5B5B5
#ifndef DATA_EEPROM_BASE
#define DATA_EEPROM_BASE ((uint32_t)0x08080000U)
#endif
#define EEPROM_REPAIR_STATUS_ADDRESS DATA_EEPROM_BASE

extern const char* SECRET_UNLOCK_PHRASE;
extern const char* CHALLENGE1_CODE;
extern const char* CHALLENGE2_CODE;
extern const char* JOHNNY5_FLAG;

#define NUM_DIAGNOSTIC_MESSAGES 30 // Ensure this matches the array definition in challenge.c
extern const char* DIAGNOSTIC_MESSAGES[NUM_DIAGNOSTIC_MESSAGES];


/* Function Prototypes */
void load_repair_status(void);
void save_repair_status(void);
void check_all_repairs_and_notify(void);
void handle_diagnostic_stream(uint32_t now); // Manages USART2 diagnostic output
void init_challenge_system(void); // For any one-time initializations

// Potentially, functions related to specific challenge interactions if they become complex
// e.g., void start_comms_challenge_scan(void);
// void process_comms_challenge_fix(const char* code);

#endif // CHALLENGE_H
