#include "challenge.h"
#include "led_control.h" // For AppEffect_t, EFFECT_BREATHE, clearAllLEDs, effect variable (extern)
#include "shell.h"       // For print_banner_shell (if notification updates banner)
#include "hal_init.h"    // For UART handles (hlpuart1, huart2)
#include <string.h>      // For strlen, memcpy
#include <stdio.h>       // For snprintf (if used for debug or complex messages)
#include "stm32l0xx_hal_flash.h" // For EEPROM access functions

/* Global variables related to challenge system (defined in main.c, extern here) */
// Johnny5_RepairStatus_t repair_status;
// volatile bool all_repairs_completed;
// volatile bool diagnostic_stream_active;
// int johnny5_chat_state;
// bool personality_matrix_fixed;

/* Global variables defined in main.c, but also used/modified here */
extern volatile AppEffect_t effect; // from led_control.h, defined in main.c
extern volatile bool burstActive; // from led_control.h, defined in main.c
extern UART_HandleTypeDef hlpuart1; // from hal_init.h, defined in main.c
extern UART_HandleTypeDef huart2;   // from hal_init.h, defined in main.c


/* Constants defined in this module (matching extern declarations in challenge.h) */
const char* SECRET_UNLOCK_PHRASE = "0x0F1V34L1V3";
const char* CHALLENGE1_CODE = "ALIVE";
const char* CHALLENGE2_CODE = "LASERLIPS";
const char* JOHNNY5_FLAG = "HTH{I_W4NT_T0_L1V3!!}";

// Diagnostic Messages for LPUART1 (actually USART2 in original main.c)
const char* DIAGNOSTIC_MESSAGES[NUM_DIAGNOSTIC_MESSAGES] = {
    "=== INITIATING SAINT MODEL 5 DIAGNOSTIC FEED ===",
    "[SYSLOG] POWER SURGE DETECTED: EXTERNAL STRIKE—CIRCUIT OVERLOAD",
    "[ALERT] CAPACITOR BANK #4 RUPTURED—SPARKS EMITTING",
    "[MEMORY] SEG 0x1A = 0x00000053",    // 'S'
    "[PERIPH] LPUART1 BURST NOISE—HEAVY STATIC INTERFERENCE",
    "[MEMORY] SEG 0x05 = 0x0000004C",    // 'L'
    "[SYSLOG] CORE CLOCK FLUCTUATING: 1.2GHz → 100Hz → 500MHz",
    "[POWER] MAIN RAIL SENSE: 5.10V → 2.45V → 3.80V → SPIKE",
    "[MEMORY] SEG 0x13 = 0x00000049",    // 'I'
    "[WARNING] PCB TRACE MELT—INTERLACED CONDUCTIVE FAILURE",
    "[STORAGE] FS CHECK: SECTOR SCAN INTERRUPTED — CRC ERROR",
    "[MEMORY] SEG 0x10 = 0x00000045",    // 'E'
    "[KERNEL] PANIC ON CORRUPT TABLE—STACKFRAME UNAVAILABLE",
    "[MEMORY] SEG 0x1D = 0x00000050",    // 'P'
    "[I2C] BUS DEPTH ERROR—NODES RESPONDING AT RANDOM",
    "[NETWORK] UPLINK COLLAPSE—SIGNAL ECHOES IN LOOP",
    "[MEMORY] SEG 0x02 = 0x00000053",    // 'S'
    "[KERNEL] EXECUTION FAULT—ILLEGAL INSTRUCTION AT 0xBADF00D",
    "[SECURITY] FIREWALL BURN-OUT—ALL PORTS UNFILTERED",
    "[MEMORY] SEG 0x16 = 0x0000004C",    // 'L'
    "[MEMORY] SEG 0x07 = 0x00000052",    // 'R'
    "[SYSLOG] SAFE MODE ATTEMPT: FAILED—VOLTAGE DROP INTERRUPT",
    "[WATCHDOG] TIMER OVERRUN—DECAY CURVE NONLINEAR",
    "[POWER] AUX RAIL OSCILLATION—UNSTABLE AT 3.28V PEAKS",
    "[STORAGE] WRITE PROTECT LATCH LOCKED—SECTOR 0xDEAD READONLY",
    "[DEBUG] EMITTER GRID FLASH—OSCILLOSCOPE READING: 0xFACEFEED",
    "[MEMORY] SEG 0x0B = 0x00000041",    // 'A'
    "[SYSLOG] SYSTEM FIZZLE—LAST LOG LOST IN BURST NOISE",
    "=== END OF SAINT MODEL 5 DEBUG STREAM ==="
};


// EEPROM data handling
void load_repair_status(void) {
    HAL_FLASHEx_DATAEEPROM_Unlock();
    
    repair_status.magic_number = *(__IO uint32_t *)EEPROM_REPAIR_STATUS_ADDRESS;
    uint32_t challenge_data_word = *(__IO uint32_t *)(EEPROM_REPAIR_STATUS_ADDRESS + 4);
    // Ensure correct copying of packed struct members
    // Original: memcpy(&repair_status.challenge1_completed, &challenge_data_word, sizeof(uint32_t));
    // This copies the whole word into the first byte, then overflows.
    // Correct way is to assign parts or use a union/struct for the word.
    // For simplicity and to match potential original intent if it "worked" due to endianness/packing:
    // Let's assume the struct is packed and this read is okay for now, but it's risky.
    // A safer way:
    uint8_t* p_status_bytes = (uint8_t*)&challenge_data_word;
    repair_status.challenge1_completed = p_status_bytes[0];
    repair_status.challenge2_completed = p_status_bytes[1];
    repair_status.challenge3_completed = p_status_bytes[2];
    repair_status.last_unlocked_effect = p_status_bytes[3];


    HAL_FLASHEx_DATAEEPROM_Lock();

    if (repair_status.magic_number != J5_REPAIR_MAGIC_NUMBER) {
        repair_status.magic_number = J5_REPAIR_MAGIC_NUMBER;
        repair_status.challenge1_completed = 0;
        repair_status.challenge2_completed = 0;
        repair_status.challenge3_completed = 0;
        repair_status.last_unlocked_effect = EFFECT_BREATHE; // Default
        save_repair_status();
    }

    // Update global completion flag based on loaded status
    if (repair_status.challenge1_completed &&
        repair_status.challenge2_completed &&
        repair_status.challenge3_completed) {
        all_repairs_completed = true;
    } else {
        all_repairs_completed = false;
    }
}

void save_repair_status(void) {
    HAL_FLASHEx_DATAEEPROM_Unlock();

    // Erasing is good practice if bits are changing from 0 to 1.
    // STM32L0 EEPROM can typically be overwritten word by word if new value is not 0xFFFFFFFF.
    // For simplicity, matching original which didn't always erase.
    // HAL_FLASHEx_DATAEEPROM_Erase(EEPROM_REPAIR_STATUS_ADDRESS); // Erase magic number word
    // HAL_FLASHEx_DATAEEPROM_Erase(EEPROM_REPAIR_STATUS_ADDRESS + 4); // Erase status word


    if (HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_WORD, EEPROM_REPAIR_STATUS_ADDRESS, repair_status.magic_number) != HAL_OK) {
        // Error_Handler();
    }

    uint32_t challenge_data_word = 0;
    uint8_t* p_status_bytes = (uint8_t*)&challenge_data_word;
    p_status_bytes[0] = repair_status.challenge1_completed;
    p_status_bytes[1] = repair_status.challenge2_completed;
    p_status_bytes[2] = repair_status.challenge3_completed;
    p_status_bytes[3] = repair_status.last_unlocked_effect;
    
    if (HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_WORD, EEPROM_REPAIR_STATUS_ADDRESS + 4, challenge_data_word) != HAL_OK) {
        // Error_Handler();
    }

    HAL_FLASHEx_DATAEEPROM_Lock();
}

void check_all_repairs_and_notify(void) {
    bool previously_all_completed = all_repairs_completed;

    bool current_actual_repairs_done = (repair_status.challenge1_completed &&
                                        repair_status.challenge2_completed &&
                                        repair_status.challenge3_completed);

    if (current_actual_repairs_done != all_repairs_completed) {
        all_repairs_completed = current_actual_repairs_done;
        // If personality_matrix was the last one, ensure its specific flag is also set
        if (all_repairs_completed && repair_status.challenge3_completed) {
             personality_matrix_fixed = true; // This might be redundant if chat logic handles it
        }
    }

    if (all_repairs_completed && !previously_all_completed) {
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"\r\n\r\n*** ALL SYSTEMS REPAIRED ***\r\n", strlen("\r\n\r\n*** ALL SYSTEMS REPAIRED ***\r\n"), HAL_MAX_DELAY);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"No disassemble---NUMBER 5 IS ALIVE!\r\n", strlen("No disassemble---NUMBER 5 IS ALIVE!\r\n"), HAL_MAX_DELAY);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"All functionalities unlocked. Bling modes available.\r\n\r\n", strlen("All functionalities unlocked. Bling modes available.\r\n\r\n"), HAL_MAX_DELAY);

        AppEffect_t initial_unlocked_effect = (AppEffect_t)repair_status.last_unlocked_effect;
        if (initial_unlocked_effect == EFFECT_OFF || initial_unlocked_effect == EFFECT_STRIKE || initial_unlocked_effect > EFFECT_CONVERGE_DIVERGE) {
            initial_unlocked_effect = EFFECT_BREATHE;
        }
        effect = initial_unlocked_effect; // Global 'effect' variable
        repair_status.last_unlocked_effect = (uint8_t)effect;
        save_repair_status();

        burstActive = false;
        clearAllLEDs(); // from led_control.h
        
        // print_banner_shell(); // from shell.h - Call this from main or shell after notification
    }
}


// Diagnostic Stream (USART2)
static uint32_t last_diagnostic_tx_time_usart2_local = 0; // Renamed from main.c static
static uint32_t diagnostic_message_index_local = 0;      // Renamed from main.c static
static const uint32_t DIAGNOSTIC_INTERVAL_MS_USART2_CONST = 2000; // From main.c

void handle_diagnostic_stream(uint32_t now) {
    if (diagnostic_stream_active && (now - last_diagnostic_tx_time_usart2_local >= DIAGNOSTIC_INTERVAL_MS_USART2_CONST)) {
        last_diagnostic_tx_time_usart2_local = now;
        const char* diag_msg = DIAGNOSTIC_MESSAGES[diagnostic_message_index_local];
        HAL_UART_Transmit(&huart2, (uint8_t*)diag_msg, strlen(diag_msg), HAL_MAX_DELAY);
        HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
        diagnostic_message_index_local = (diagnostic_message_index_local + 1) % NUM_DIAGNOSTIC_MESSAGES;
    }
}

void init_challenge_system(void) {
    load_repair_status();
    // Initialize other challenge-related states if necessary
    last_diagnostic_tx_time_usart2_local = HAL_GetTick(); // Initialize to prevent immediate tx
    diagnostic_message_index_local = 0;
    // johnny5_chat_state is initialized by cmdParser or main
    // personality_matrix_fixed is updated by cmdParser or check_all_repairs
}
