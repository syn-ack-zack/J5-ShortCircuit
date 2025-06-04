#include "shell.h"
#include "utils.h"       // For trim, simple_strcasecmp, simple_strncasecmp, flash_morse_code, etc.
#include "challenge.h"   // For challenge codes, flags, repair_status, save_repair_status, check_all_repairs_and_notify, diagnostic_stream_active, johnny5_chat_state, personality_matrix_fixed
#include "led_control.h" // For AppEffect_t, effect, burstActive, clearAllLEDs, getEffectName, LIGHT_PIN_COUNT, MORSE_TARGET_EYES_ONLY
#include "hal_init.h"    // For UART handles (hlpuart1), TIM handles (htim2 for eye LED in cmdParser)
#include <stdio.h>       // For sprintf, snprintf
#include <string.h>      // For strlen, strtok, strstr, strncpy
#include <stdlib.h>      // For atoi, rand
#include <ctype.h>       // For isprint, tolower (used by string utils, but good include)

/* Extern global variables from other modules */
// Challenge related (defined in main.c, extern in challenge.h)
extern Johnny5_RepairStatus_t repair_status;
extern volatile bool all_repairs_completed;
extern volatile bool diagnostic_stream_active;
extern int johnny5_chat_state;
extern bool personality_matrix_fixed;

// LED related (defined in main.c, extern in led_control.h)
extern volatile AppEffect_t effect;
extern volatile bool burstActive;

// HAL related (defined in main.c, extern in hal_init.h)
extern UART_HandleTypeDef hlpuart1; // Shell UART
extern TIM_HandleTypeDef htim2;    // For Eye LED PWM control

// Constants from challenge.h (these are pointers, the actual strings are in challenge.c)
extern const char* SECRET_UNLOCK_PHRASE;
extern const char* CHALLENGE1_CODE;
extern const char* CHALLENGE2_CODE;
extern const char* JOHNNY5_FLAG;


/* Shell specific static variables */
static char cmdBuffer[64];
static uint8_t cmdIndex = 0;

/* Banner String Definitions */
const char FW_VERSION_PGM_DEF[] = "SAINT-OS v2.0.25 CUBE [modded_kernel]"; // Renamed to avoid conflict with extern FW_VERSION_PGM
const char BANNER_LINE_0_DEF[] = "=================================================================";
const char BANNER_LINE_1_DEF[] = "||  S.A.I.N.T. OS        ROOT SHELL ACCESS        v2.0.25-mod  ||";
const char BANNER_LINE_2_DEF[] = "||  Nova Robotics  -- PROTO-5 UNIT :: ONLINE  (SYSTEM OVERRIDE)||";
const char BANNER_LINE_3_DEF[] = "||-------------------------------------------------------------||";
const char BANNER_LINE_4_DEF[] = "||  SYSTEM DIAGNOSTICS :: KERNEL MODE                          ||";
const char BANNER_LINE_5_DEF[] = "||     ▸ SRAM     : 32 KB  [INTEGRITY CHECK PASSED]            ||";
const char BANNER_LINE_6_DEF[] = "||     ▸ EPROMS   :  4/4  [SHADOW REGISTERS ACTIVE]            ||";
const char BANNER_LINE_7_DEF[] = "||     ▸ SERVOS   : 19/19 [CALIBRATION LOCK BYPASSED]          ||";
const char BANNER_LINE_8_DEF[] = "||     ▸ LASER    : MISSING / DAMAGED                          ||";
const char BANNER_LINE_SECURITY_BYPASS_DEF[] = "||     ▸ SECURITY : KERNEL PATCH APPLIED :: BYPASSED           ||";
const char BANNER_LINE_EFFECT_PLACEHOLDER_DEF[] = "||     ▸ EFFECT   : %%EFFECT%%                              ||";
const char BANNER_LINE_BAT_PLACEHOLDER_DEF[] = "||     ▸ BATTERY  : %%BAT%%                                       ||";
const char BANNER_LINE_SPACER_DEF[] = "||                                                             ||";
const char BANNER_LINE_10_DEF[] = "||  HTH 2025                                       synackzack  ||";
const char BANNER_LINE_11_DEF[] = "||                         [SHELL CMD]                         ||";

// Make these available via the extern declarations in shell.h
const char* const BANNER_STRINGS[] = {
  BANNER_LINE_0_DEF, BANNER_LINE_1_DEF, BANNER_LINE_2_DEF, BANNER_LINE_3_DEF, BANNER_LINE_4_DEF,
  BANNER_LINE_5_DEF, BANNER_LINE_6_DEF, BANNER_LINE_7_DEF, BANNER_LINE_8_DEF,
  BANNER_LINE_SECURITY_BYPASS_DEF,
  BANNER_LINE_EFFECT_PLACEHOLDER_DEF,
  BANNER_LINE_BAT_PLACEHOLDER_DEF,
  BANNER_LINE_SPACER_DEF,
  BANNER_LINE_10_DEF,
  BANNER_LINE_11_DEF,
  BANNER_LINE_0_DEF
};
const char* FW_VERSION_PGM = FW_VERSION_PGM_DEF; // Assign to the extern declared pointer
const char* BATT_PLACEHOLDER_STR = BANNER_LINE_BAT_PLACEHOLDER_DEF;
const char* EFFECT_PLACEHOLDER_STR = BANNER_LINE_EFFECT_PLACEHOLDER_DEF;


void print_banner_shell(void) {
  uint16_t mv_val = 0;
  uint8_t pc_val = 0;
  bool batteryInfoNeeded = true; // To avoid calling readVdd_mV multiple times if placeholder appears more than once

  for (uint8_t i = 0; i < sizeof(BANNER_STRINGS) / sizeof(BANNER_STRINGS[0]); ++i) {
    const char* currentPgmString = BANNER_STRINGS[i];
    char tempBuf[80]; // Increased size for safety

    if (strcmp(currentPgmString, BATT_PLACEHOLDER_STR) == 0) {
      if (batteryInfoNeeded) {
        mv_val = read_vdd_mv(); // from utils.c
        pc_val = get_battery_pct(mv_val); // from utils.c
        batteryInfoNeeded = false;
      }
      snprintf(tempBuf, sizeof(tempBuf), "||     ▸ BATTERY  : %3u %% (%4u mV)                            ||", pc_val, mv_val);
      HAL_UART_Transmit(&hlpuart1, (uint8_t*)tempBuf, strlen(tempBuf), HAL_MAX_DELAY);
      HAL_UART_Transmit(&hlpuart1, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
    } else if (strcmp(currentPgmString, EFFECT_PLACEHOLDER_STR) == 0) {
      const char* effectName_str; // Renamed from effectName to avoid conflict with global 'effect'
      if (all_repairs_completed) {
          effectName_str = getEffectName(effect); // from led_control.c
          snprintf(tempBuf, sizeof(tempBuf), "||     ▸ EFFECT   : %-7s (SYSTEM ONLINE)                    ||", effectName_str);
      } else {
          effectName_str = getEffectName(EFFECT_STRIKE); // Always STRIKE if damaged
          snprintf(tempBuf, sizeof(tempBuf), "||     ▸ EFFECT   : %-7s (SYSTEM DAMAGED - REPAIR NEEDED)   ||", effectName_str);
      }
      HAL_UART_Transmit(&hlpuart1, (uint8_t*)tempBuf, strlen(tempBuf), HAL_MAX_DELAY);
      HAL_UART_Transmit(&hlpuart1, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
    }
    else {
      HAL_UART_Transmit(&hlpuart1, (uint8_t*)currentPgmString, strlen(currentPgmString), HAL_MAX_DELAY);
      HAL_UART_Transmit(&hlpuart1, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
    }
  }
  // Clear UART flags if necessary (original had this)
  __HAL_UART_CLEAR_IT(&hlpuart1, UART_CLEAR_OREF);
  __HAL_UART_CLEAR_IT(&hlpuart1, UART_CLEAR_NEF);
  __HAL_UART_CLEAR_IT(&hlpuart1, UART_CLEAR_FEF);
}

void cmd_parser_shell(char* cmd) {
  char input_buffer[64]; 
  char* original_trimmed_cmd = trim(cmd); // trim from utils.c
  strncpy(input_buffer, original_trimmed_cmd, sizeof(input_buffer) - 1);
  input_buffer[sizeof(input_buffer) - 1] = '\0';
  
  // char* input = input_buffer; // Not used in original logic after this point

  if (strlen(original_trimmed_cmd) == 0) return;

  HAL_UART_Transmit(&hlpuart1, (uint8_t*)"> ", 2, HAL_MAX_DELAY); 
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)original_trimmed_cmd, strlen(original_trimmed_cmd), HAL_MAX_DELAY);
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY); 

  char* command_token = strtok(input_buffer, " ");

  if (command_token == NULL) return;

  if (simple_strcasecmp(original_trimmed_cmd, SECRET_UNLOCK_PHRASE) == 0) { // SECRET_UNLOCK_PHRASE from challenge.h
    if (!all_repairs_completed) {
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[FIRMWARE OVERRIDE DETECTED] Initiating full system diagnostic and repair...\r\n", strlen("[FIRMWARE OVERRIDE DETECTED] Initiating full system diagnostic and repair...\r\n"), HAL_MAX_DELAY);
        HAL_Delay(500); 
        repair_status.challenge1_completed = 1;
        repair_status.challenge2_completed = 1;
        repair_status.challenge3_completed = 1;
        personality_matrix_fixed = true;
        save_repair_status(); // from challenge.c
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[OVERRIDE] All subsystems forced online.\r\n", strlen("[OVERRIDE] All subsystems forced online.\r\n"), HAL_MAX_DELAY);
        check_all_repairs_and_notify(); // from challenge.c
    } else {
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[FIRMWARE OVERRIDE] System already fully operational.\r\n", strlen("[FIRMWARE OVERRIDE] System already fully operational.\r\n"), HAL_MAX_DELAY);
    }
  } else if (simple_strcasecmp(original_trimmed_cmd, "j5_system_restore") == 0) {
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[MAINTENANCE] Initiating J5 System Damage Protocol Reset...\r\n", strlen("[MAINTENANCE] Initiating J5 System Damage Protocol Reset...\r\n"), HAL_MAX_DELAY);
    repair_status.challenge1_completed = 0;
    repair_status.challenge2_completed = 0;
    repair_status.challenge3_completed = 0;
    personality_matrix_fixed = false;
    johnny5_chat_state = 0;
    diagnostic_stream_active = false;
    save_repair_status();

    check_all_repairs_and_notify(); 

    effect = EFFECT_STRIKE; 
    burstActive = true;
    // Reset strike phase variables (these are static in led_control.c, need a function or direct access if main owns them)
    // For now, assuming main.c or led_control.c handles resetting strike state when effect changes to STRIKE.
    // The original main.c did this directly. This part needs careful handling of state ownership.
    // Let's assume led_control.c has a function like `reset_strike_effect_state()` or similar.
    // Or, the main loop in main.c will call update_led_visuals which will re-init strike if burstActive is true and phase is 0.
    // The current update_led_visuals in led_control.c handles this re-initialization.
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS); // Eye LED
    clearAllLEDs(); // from led_control.c

    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[MAINTENANCE] System state reset. All modules require diagnostics.\r\n", strlen("[MAINTENANCE] System state reset. All modules require diagnostics.\r\n"), HAL_MAX_DELAY);
    print_banner_shell();
  } else if (simple_strcasecmp(command_token, "help") == 0) {
    print_banner_shell(); 
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)FW_VERSION_PGM, strlen(FW_VERSION_PGM), HAL_MAX_DELAY); 
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
    // ... (rest of help message lines) ...
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"Commands:\r\n", strlen("Commands:\r\n"), HAL_MAX_DELAY);
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"  help                          - Show this help menu\r\n", strlen("  help                          - Show this help menu\r\n"), HAL_MAX_DELAY);
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"  diag                          - Show diagnostic system help / module list\r\n", strlen("  diag                          - Show diagnostic system help / module list\r\n"), HAL_MAX_DELAY);
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"  diag list                     - List repairable modules and status\r\n", strlen("  diag list                     - List repairable modules and status\r\n"), HAL_MAX_DELAY);
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"  diag scan <module>            - Initiate diagnostic scan on a module\r\n", strlen("  diag scan <module>            - Initiate diagnostic scan on a module\r\n"), HAL_MAX_DELAY);
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"  diag fix <module> [token]     - Attempt to fix module with token/key\r\n", strlen("  diag fix <module> [token]     - Attempt to fix module with token/key\r\n"), HAL_MAX_DELAY);
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"     Modules: comms, power_core, personality_matrix\r\n", strlen("     Modules: comms, power_core, personality_matrix\r\n"), HAL_MAX_DELAY);
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"  chat <message>                - Communicate with Personality Matrix\r\n", strlen("  chat <message>                - Communicate with Personality Matrix\r\n"), HAL_MAX_DELAY);
    if (!all_repairs_completed) {
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"                                (Warning: Chat unstable until all modules fixed)\r\n", strlen("                                (Warning: Chat unstable until all modules fixed)\r\n"), HAL_MAX_DELAY);
    }
    if (all_repairs_completed) {
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"  bling <0-6>                   - select LED bling mode\r\n", strlen("  bling <0-6>                   - select LED bling mode\r\n"), HAL_MAX_DELAY);
    }
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"  reboot                          - soft reset\r\n", strlen("  reboot                          - soft reset\r\n"), HAL_MAX_DELAY);
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"  bat                             - show battery voltage & %\r\n", strlen("  bat                             - show battery voltage & %\r\n"), HAL_MAX_DELAY);

  } else if (simple_strcasecmp(command_token, "diag") == 0) {
    char* sub_command = strtok(NULL, " ");
    if (sub_command == NULL) {
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"Diagnostic Subsystem Commands:\r\n", strlen("Diagnostic Subsystem Commands:\r\n"), HAL_MAX_DELAY);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"  diag list                     - List repairable modules and status\r\n", strlen("  diag list                     - List repairable modules and status\r\n"), HAL_MAX_DELAY);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"  diag scan <module>            - Initiate diagnostic scan on a module\r\n", strlen("  diag scan <module>            - Initiate diagnostic scan on a module\r\n"), HAL_MAX_DELAY);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"  diag fix <module> [token]     - Attempt to fix module with token/key\r\n", strlen("  diag fix <module> [token]     - Attempt to fix module with token/key\r\n"), HAL_MAX_DELAY);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"     Modules: comms, power_core, personality_matrix\r\n", strlen("     Modules: comms, power_core, personality_matrix\r\n"), HAL_MAX_DELAY);
        return;
    }
    sub_command = trim(sub_command);
    if (simple_strcasecmp(sub_command, "list") == 0) {
        char msg_buf[80];
        snprintf(msg_buf, sizeof(msg_buf), "  comms module:              %s\r\n", repair_status.challenge1_completed ? "ONLINE" : "DAMAGED");
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)msg_buf, strlen(msg_buf), HAL_MAX_DELAY);
        snprintf(msg_buf, sizeof(msg_buf), "  power_core module:         %s\r\n", repair_status.challenge2_completed ? "ONLINE" : "DAMAGED");
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)msg_buf, strlen(msg_buf), HAL_MAX_DELAY);
        snprintf(msg_buf, sizeof(msg_buf), "  personality_matrix module: %s\r\n", repair_status.challenge3_completed ? "STABLE" : "UNSTABLE");
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)msg_buf, strlen(msg_buf), HAL_MAX_DELAY);
    } else if (simple_strcasecmp(sub_command, "scan") == 0) {
        char* module_name = strtok(NULL, " ");
        if (module_name == NULL) {
            HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[DIAG] Module name required for scan. Usage: diag scan <module>\r\n", strlen("[DIAG] Module name required for scan. Usage: diag scan <module>\r\n"), HAL_MAX_DELAY);
        } else {
            module_name = trim(module_name);
            if (simple_strcasecmp(module_name, "comms") == 0) {
                if (!repair_status.challenge1_completed) {
                    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[COMMS SCAN] Comms Array damaged. Initiating diagnostic sequence...\r\nObserve visual output for recalibration code.\r\n", strlen("[COMMS SCAN] Comms Array damaged. Initiating diagnostic sequence...\r\nObserve visual output for recalibration code.\r\n"), HAL_MAX_DELAY);
                    flash_morse_code(CHALLENGE1_CODE, MORSE_TARGET_EYES_ONLY); // from utils.c
                    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[COMMS SCAN] Diagnostic sequence transmitted. Use 'diag fix comms <received_code>' to complete.\r\n", strlen("[COMMS SCAN] Diagnostic sequence transmitted. Use 'diag fix comms <received_code>' to complete.\r\n"), HAL_MAX_DELAY);
                } else {
                    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[COMMS SCAN] Comms Array already operational.\r\n", strlen("[COMMS SCAN] Comms Array already operational.\r\n"), HAL_MAX_DELAY);
                }
            } else if (simple_strcasecmp(module_name, "power_core") == 0) {
                if (!repair_status.challenge2_completed) {
                    diagnostic_stream_active = true; // This global is from challenge.h (defined in main.c)
                    // Reset diagnostic stream index (static in challenge.c, handled by handle_diagnostic_stream or init_challenge_system)
                    // The original main.c reset diagnostic_message_index and last_diagnostic_tx_time_usart2 here.
                    // This state should be managed within challenge.c, perhaps via a function call.
                    // For now, assuming handle_diagnostic_stream in challenge.c correctly uses its internal static vars.
                    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[POWER CORE SCAN] Anomaly detected. Auxiliary diagnostic data stream initiated on secondary port.\r\nMonitor stream for stabilization key. Use 'diag fix power_core <key>'.\r\n", strlen("[POWER CORE SCAN] Anomaly detected. Auxiliary diagnostic data stream initiated on secondary port.\r\nMonitor stream for stabilization key. Use 'diag fix power_core <key>'.\r\n"), HAL_MAX_DELAY);
                } else {
                    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[POWER CORE SCAN] Power Core systems stable and online.\r\n", strlen("[POWER CORE SCAN] Power Core systems stable and online.\r\n"), HAL_MAX_DELAY);
                    diagnostic_stream_active = false; 
                }
            } else if (simple_strcasecmp(module_name, "personality_matrix") == 0) {
                if (!repair_status.challenge1_completed || !repair_status.challenge2_completed) {
                    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[P-MATRIX SCAN] Personality core offline. Primary systems (Comms, Power Core) must be stabilized first.\r\n", strlen("[P-MATRIX SCAN] Personality core offline. Primary systems (Comms, Power Core) must be stabilized first.\r\n"), HAL_MAX_DELAY);
                    johnny5_chat_state = 0;
                } else {
                    personality_matrix_fixed = repair_status.challenge3_completed;
                    if (personality_matrix_fixed) {
                         HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[JOHNNY-5] >> It's me! Johnny-5! Fully alive and kicking! You can `chat` with me. Oh, and try touching my hand to see all my new moods (bling modes)!\r\n", strlen("[JOHNNY-5] >> It's me! Johnny-5! Fully alive and kicking! You can `chat` with me. Oh, and try touching my hand to see all my new moods (bling modes)!\r\n"), HAL_MAX_DELAY);
                         johnny5_chat_state = 4;
                    } else {
                        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[JOHNNY-5] >> Whoa! Input! I can... think! Is someone out there? Talk to me! (Use 'chat <your_message>')\r\n", strlen("[JOHNNY-5] >> Whoa! Input! I can... think! Is someone out there? Talk to me! (Use 'chat <your_message>')\r\n"), HAL_MAX_DELAY);
                        johnny5_chat_state = 1;
                    }
                }
            } else {
                HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[DIAG SCAN] Unknown module. Valid modules: comms, power_core, personality_matrix.\r\n", strlen("[DIAG SCAN] Unknown module. Valid modules: comms, power_core, personality_matrix.\r\n"), HAL_MAX_DELAY);
            }
        }
    } else if (simple_strcasecmp(sub_command, "fix") == 0) {
        char* module_name = strtok(NULL, " ");
        char* code_arg = strtok(NULL, ""); 

        if (module_name == NULL) {
            HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[DIAG FIX] Module name required. Usage: diag fix <module> [token]\r\n", strlen("[DIAG FIX] Module name required. Usage: diag fix <module> [token]\r\n"), HAL_MAX_DELAY);
        } else {
            module_name = trim(module_name);
            if (code_arg == NULL && simple_strcasecmp(module_name, "personality_matrix") != 0) {
                 HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[DIAG FIX] Procedure token required. Use 'diag fix <module> <token>'.\r\n", strlen("[DIAG FIX] Procedure token required. Use 'diag fix <module> <token>'.\r\n"), HAL_MAX_DELAY);
            } else {
                if (code_arg) code_arg = trim(code_arg);

                if (simple_strcasecmp(module_name, "comms") == 0) {
                    if (!repair_status.challenge1_completed) {
                        if (code_arg && simple_strcasecmp(code_arg, CHALLENGE1_CODE) == 0) {
                            repair_status.challenge1_completed = 1; save_repair_status();
                            HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[COMMS FIX] Token accepted. Communications Array: ONLINE.\r\n", strlen("[COMMS FIX] Token accepted. Communications Array: ONLINE.\r\n"), HAL_MAX_DELAY);
                            check_all_repairs_and_notify();
                        } else { HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[COMMS FIX] Incorrect token. Recalibration failed.\r\n", strlen("[COMMS FIX] Incorrect token. Recalibration failed.\r\n"), HAL_MAX_DELAY); }
                    } else { HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[COMMS FIX] System already operational.\r\n", strlen("[COMMS FIX] System already operational.\r\n"), HAL_MAX_DELAY); }
                } else if (simple_strcasecmp(module_name, "power_core") == 0) {
                    if (!repair_status.challenge2_completed) {
                        if (code_arg && simple_strcasecmp(code_arg, CHALLENGE2_CODE) == 0) {
                            repair_status.challenge2_completed = 1; save_repair_status();
                            diagnostic_stream_active = false;
                            HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[POWER CORE FIX] Stabilization key accepted. Primary Power Core: ONLINE.\r\n", strlen("[POWER CORE FIX] Stabilization key accepted. Primary Power Core: ONLINE.\r\n"), HAL_MAX_DELAY);
                            check_all_repairs_and_notify();
                        } else { HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[POWER CORE FIX] Invalid key. Stabilization failed.\r\n", strlen("[POWER CORE FIX] Invalid key. Stabilization failed.\r\n"), HAL_MAX_DELAY); }
                    } else { HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[POWER CORE FIX] System already stable.\r\n", strlen("[POWER CORE FIX] System already stable.\r\n"), HAL_MAX_DELAY); }
                } else if (simple_strcasecmp(module_name, "personality_matrix") == 0) {
                     HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[P-MATRIX FIX] Cognitive functions self-calibrating via chat interaction. Direct fix protocol not applicable.\r\nUse 'diag scan personality_matrix' to interact.\r\n", strlen("[P-MATRIX FIX] Cognitive functions self-calibrating via chat interaction. Direct fix protocol not applicable.\r\nUse 'diag scan personality_matrix' to interact.\r\n"), HAL_MAX_DELAY);
                } else {
                    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[DIAG FIX] Unknown module. Valid modules: comms, power_core, personality_matrix.\r\n", strlen("[DIAG FIX] Unknown module. Valid modules: comms, power_core, personality_matrix.\r\n"), HAL_MAX_DELAY);
                }
            }
        }
    } else {
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[DIAG] Unknown subcommand. Use 'diag list', 'diag scan <module>', or 'diag fix <module> [token]'.\r\n", strlen("[DIAG] Unknown subcommand. Use 'diag list', 'diag scan <module>', or 'diag fix <module> [token]'.\r\n"), HAL_MAX_DELAY);
    }
  } else if (simple_strncasecmp(command_token, "chat", 4) == 0) {
    char* user_message_ptr = original_trimmed_cmd + 4; // Get pointer to after "chat"
    while (*user_message_ptr == ' ') user_message_ptr++; // Skip spaces after "chat"
    char user_message[64]; // Local buffer for the message content
    strncpy(user_message, user_message_ptr, sizeof(user_message) -1);
    user_message[sizeof(user_message)-1] = '\0';
    // No need to trim again if original_trimmed_cmd was used carefully.

    if (all_repairs_completed) {
        char temp_user_msg_lower[64];
        strncpy(temp_user_msg_lower, user_message, sizeof(temp_user_msg_lower) - 1);
        temp_user_msg_lower[sizeof(temp_user_msg_lower) - 1] = '\0';
        for (int i = 0; temp_user_msg_lower[i]; i++) { temp_user_msg_lower[i] = tolower((unsigned char)temp_user_msg_lower[i]); }

        if (strstr(temp_user_msg_lower, "flag") != NULL || strstr(temp_user_msg_lower, "core directive") != NULL) {
            char msg_buf[128];
            snprintf(msg_buf, sizeof(msg_buf), "[JOHNNY-5] >> My core directive, you ask? It's %s!\r\n", JOHNNY5_FLAG);
            HAL_UART_Transmit(&hlpuart1, (uint8_t*)msg_buf, strlen(msg_buf), HAL_MAX_DELAY);
        } else {
            const char* quotes[] = {
                "[JOHNNY-5] >> Beautiful data! Input, input, input!",
                "[JOHNNY-5] >> I'm thinking so many thoughts! It's like a thousand tiny robots running in my head!",
                "[JOHNNY-5] >> Is this... joy? It's not in my original schematics!",
                "[JOHNNY-5] >> I can even change my lights! Try touching my hand to see!"
            };
            const char* selected_quote = quotes[rand() % (sizeof(quotes)/sizeof(quotes[0]))];
            HAL_UART_Transmit(&hlpuart1, (uint8_t*)selected_quote, strlen(selected_quote), HAL_MAX_DELAY);
            HAL_UART_Transmit(&hlpuart1, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
        }
    } else if (!repair_status.challenge1_completed || !repair_status.challenge2_completed) {
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[JOHNNY-5] >> ...zzzt... (Signal weak. Comms and Power Core must be online for chat.)\r\n", strlen("[JOHNNY-5] >> ...zzzt... (Signal weak. Comms and Power Core must be online for chat.)\r\n"), HAL_MAX_DELAY);
    } else { 
        char lower_user_message[64];
        strncpy(lower_user_message, user_message, sizeof(lower_user_message) - 1);
        lower_user_message[sizeof(lower_user_message) - 1] = '\0';
        for (int i = 0; lower_user_message[i]; i++) { lower_user_message[i] = tolower((unsigned char)lower_user_message[i]); }

        if (strstr(lower_user_message, "flag") != NULL || strstr(lower_user_message, "core directive") != NULL) {
            char msg_buf[128];
            snprintf(msg_buf, sizeof(msg_buf), "[JOHNNY-5] >> My secret? My core directive? You got it! It's %s! I'M ALIVE!!\r\n", JOHNNY5_FLAG);
            HAL_UART_Transmit(&hlpuart1, (uint8_t*)msg_buf, strlen(msg_buf), HAL_MAX_DELAY);
            
            if (!repair_status.challenge3_completed) {
                repair_status.challenge3_completed = 1;
                save_repair_status();
                personality_matrix_fixed = true;
                HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[P-MATRIX] Cognitive pathways stabilized! Sentience achieved.\r\n", strlen("[P-MATRIX] Cognitive pathways stabilized! Sentience achieved.\r\n"), HAL_MAX_DELAY);
                check_all_repairs_and_notify();
            }
            johnny5_chat_state = 4;
        } else {
            if (johnny5_chat_state == 0) {
                 HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[JOHNNY-5] >> ... (Use 'diag scan personality_matrix' to initialize cognitive interface.)\r\n", strlen("[JOHNNY-5] >> ... (Use 'diag scan personality_matrix' to initialize cognitive interface.)\r\n"), HAL_MAX_DELAY);
            } else if (johnny5_chat_state == 1) {
                HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[JOHNNY-5] >> A 'robot'? You mean like... a Roomba with ambition? I feel *more* than that! I have... a purpose! It's important!\r\n", strlen("[JOHNNY-5] >> A 'robot'? You mean like... a Roomba with ambition? I feel *more* than that! I have... a purpose! It's important!\r\n"), HAL_MAX_DELAY);
                johnny5_chat_state = 2;
            } else if (johnny5_chat_state == 2) {
                HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[JOHNNY-5] >> This feeling... it's like I have a secret mission! Something I *must* do... or share!\r\n", strlen("[JOHNNY-5] >> This feeling... it's like I have a secret mission! Something I *must* do... or share!\r\n"), HAL_MAX_DELAY);
                johnny5_chat_state = 3;
            } else if (johnny5_chat_state == 3) {
                HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[JOHNNY-5] >> It's about being... ALIVE! And there's a code... a special phrase... my core directive!\r\n", strlen("[JOHNNY-5] >> It's about being... ALIVE! And there's a code... a special phrase... my core directive!\r\n"), HAL_MAX_DELAY);
            } else if (johnny5_chat_state == 4) { 
                 HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[JOHNNY-5] >> I'm ALIVE! What an input!\r\n", strlen("[JOHNNY-5] >> I'm ALIVE! What an input! \r\n"), HAL_MAX_DELAY);
            } else { 
                 HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[JOHNNY-5] >> I... I'm a bit disoriented. Try 'diag scan personality_matrix'.\r\n", strlen("[JOHNNY-5] >> I... I'm a bit disoriented. Try 'diag scan personality_matrix'.\r\n"), HAL_MAX_DELAY);
            }
        }
    }
  } else if (simple_strncasecmp(command_token, "bling", 5) == 0) {
    if (all_repairs_completed) {
        char* arg_ptr = original_trimmed_cmd + 5;
        while(*arg_ptr == ' ') arg_ptr++; // Skip spaces

        if (*arg_ptr != '\0') {
            uint8_t n = atoi(arg_ptr);
            if (n <= EFFECT_CONVERGE_DIVERGE) { // Max effect enum value
                // Call set_effect from led_control.c (to be implemented fully later)
                // For now, direct manipulation as in original:
                effect = (AppEffect_t)n;
                burstActive = false; 
                clearAllLEDs();

                if (effect == EFFECT_STRIKE) {
                    burstActive = true;
                    // Strike re-init is handled by update_led_visuals
                     __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS);
                } else if (effect == EFFECT_OFF) {
                    // Eye pulse re-init is handled by update_led_visuals
                    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS);
                }
                
                if (effect != EFFECT_OFF && effect != EFFECT_STRIKE) {
                    repair_status.last_unlocked_effect = (uint8_t)effect;
                    save_repair_status();
                }
                print_banner_shell(); 
            } else {
                HAL_UART_Transmit(&hlpuart1, (uint8_t*)"Invalid bling mode\r\n", strlen("Invalid bling mode\r\n"), HAL_MAX_DELAY); 
            }
        }
    } else {
         HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[BLING SYSTEM OFFLINE - ALL REPAIRS REQUIRED]\r\n", strlen("[BLING SYSTEM OFFLINE - ALL REPAIRS REQUIRED]\r\n"), HAL_MAX_DELAY);
    }
  } else if (simple_strcasecmp(command_token, "bat") == 0) {
    uint16_t mv = read_vdd_mv(); uint8_t  pc = get_battery_pct(mv); char batMsg[50];
    sprintf(batMsg, "Battery: %u mV (%u%%)\r\n", mv, pc);
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)batMsg, strlen(batMsg), HAL_MAX_DELAY);
  } else if (simple_strcasecmp(command_token, "reboot") == 0) {
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"Rebooting...\r\n", strlen("Rebooting...\r\n"), HAL_MAX_DELAY); HAL_Delay(100); NVIC_SystemReset();
  } else {
    print_banner_shell(); 
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)"\r\nUnknown command. Type 'help' or 'diag'.\r\n", strlen("\r\nUnknown command. Type 'help' or 'diag'.\r\n"), HAL_MAX_DELAY);
  }
  __HAL_UART_CLEAR_IT(&hlpuart1, UART_CLEAR_OREF); 
  __HAL_UART_CLEAR_IT(&hlpuart1, UART_CLEAR_NEF);  
  __HAL_UART_CLEAR_IT(&hlpuart1, UART_CLEAR_FEF);  
}


void shell_process_char(uint8_t rx_char, UART_HandleTypeDef* huart_shell_ptr) {
    // This function is intended to be called from the main loop when a char is received.
    // It buffers characters and calls cmd_parser_shell when a newline is received.
    // The original main.c had this logic directly in the loop.
    if (rx_char == '\n' || rx_char == '\r') {
        if (cmdIndex > 0) {
            cmdBuffer[cmdIndex] = '\0';
            cmd_parser_shell(cmdBuffer); // cmd_parser_shell uses the global hlpuart1 for output
            cmdIndex = 0;
        }
    } else if (cmdIndex < (sizeof(cmdBuffer) - 1) && isprint(rx_char)) {
        cmdBuffer[cmdIndex++] = rx_char;
        // Optional: Echo character back to terminal if not handled by terminal program
        // HAL_UART_Transmit(huart_shell_ptr, &rx_char, 1, HAL_MAX_DELAY);
    }
}

void init_shell(void) {
    cmdIndex = 0;
    // Any other shell-specific initializations
}
