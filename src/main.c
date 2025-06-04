/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Johnny-5 Badge (HTH 2025)
  * @author         : synackzack 
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32l0xx_hal.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include "hal_init.h"
#include "led_control.h"
#include "challenge.h"
#include "shell.h"
#include "utils.h"
#include "sw_pwm.h"

/* Global Variable Definitions (declared extern in module headers) */

// HAL Handles (used by hal_init.c and others)
UART_HandleTypeDef hlpuart1; // LPUART1 for Shell
UART_HandleTypeDef huart2;   // USART2 for Diagnostic Stream
ADC_HandleTypeDef hadc;
TIM_HandleTypeDef htim2;  // Main PWM timer (Eye LED, Light Bar HW PWM)
TIM_HandleTypeDef htim21; // Generic Timer (if used beyond base init)
TIM_HandleTypeDef htim22; // Light Bar HW PWM

// Challenge System State (used by challenge.c, shell.c)
Johnny5_RepairStatus_t repair_status;
volatile bool all_repairs_completed = false;
volatile bool diagnostic_stream_active = false;
int johnny5_chat_state = 0;
bool personality_matrix_fixed = false;

// LED Effect State (used by led_control.c, shell.c, utils.c for flashMorse)
volatile AppEffect_t effect = EFFECT_STRIKE; // Initial effect
volatile bool burstActive = false;           // For STRIKE effect

// Constants defined in their respective .c files and extern'd in .h
// e.g. SECRET_UNLOCK_PHRASE is in challenge.c, extern in challenge.h

/* Private function prototypes -----------------------------------------------*/
// SysTick_Handler is now the only "private" function prototype here, rest are in modules.
void SysTick_Handler(void);


int main(void)
{
  HAL_Init(); // Initializes Flash interface, Systick, etc.
  SystemClock_Config(); // from hal_init.c
  srand(HAL_GetTick()); // Seed random number generator

  // Release SWO pin PB3 for GPIO use if not debugging
  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitTypeDef gpio_init_swo_release = {0};
  gpio_init_swo_release.Pin = GPIO_PIN_3; // PB3
  gpio_init_swo_release.Mode = GPIO_MODE_ANALOG; // Set to analog to free it
  gpio_init_swo_release.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &gpio_init_swo_release);

  /* Initialize all configured peripherals */
  MX_GPIO_Init();         // from hal_init.c
  MX_LPUART1_UART_Init(); // Shell UART, from hal_init.c
  MX_USART2_UART_Init();  // Diagnostic UART, from hal_init.c
  MX_ADC_Init();          // from hal_init.c
  MX_TIM2_Init();         // from hal_init.c
  MX_TIM21_Init();        // from hal_init.c
  MX_TIM22_Init();        // from hal_init.c

  init_software_pwm();    // from sw_pwm.c
  init_challenge_system(); // from challenge.c (loads repair_status, sets initial all_repairs_completed)
  init_shell();           // from shell.c
  init_led_effects();     // from led_control.c (currently empty, but good practice)


  // Determine initial LED effect based on loaded repair status
  if (!all_repairs_completed) {
      effect = EFFECT_STRIKE;
      burstActive = true;
      // Strike phase variables are static within led_control.c and will be reset by update_led_visuals
      __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS); // Ensure eye is on for strike
  } else {
      AppEffect_t loaded_effect = (AppEffect_t)repair_status.last_unlocked_effect;
      if (loaded_effect == EFFECT_OFF || loaded_effect == EFFECT_STRIKE || loaded_effect > EFFECT_CONVERGE_DIVERGE) {
          loaded_effect = EFFECT_BREATHE;
          repair_status.last_unlocked_effect = (uint8_t)loaded_effect;
          save_repair_status(); // from challenge.c
      }
      effect = loaded_effect;
      burstActive = false;
  }

  // Start PWM channels
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1); // Eye LED
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2); // Light Bar HW PWM (e.g. LIGHT_PINS[0] -> PA1)
  HAL_TIM_PWM_Start(&htim22, TIM_CHANNEL_1); // Light Bar HW PWM (e.g. LIGHT_PINS[3] -> PA6)
  // TIM21 is not started here for PWM, assuming it's for other purposes or started if specific channels are used.

  // Initial Shell Output
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)FW_VERSION_PGM, strlen(FW_VERSION_PGM), HAL_MAX_DELAY); // FW_VERSION_PGM from shell.c/h
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
  print_banner_shell(); // from shell.c
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)"Type 'help' for commands.\r\n\r\n", strlen("Type 'help' for commands.\r\n\r\n"), HAL_MAX_DELAY);

  // Clear IT flags for LPUART1 (shell)
  __HAL_UART_CLEAR_IT(&hlpuart1, UART_CLEAR_OREF);
  __HAL_UART_CLEAR_IT(&hlpuart1, UART_CLEAR_NEF);
  __HAL_UART_CLEAR_IT(&hlpuart1, UART_CLEAR_FEF);


  // Variables for main loop
  static bool lastPressed_cap = false;
  static uint32_t last_touch_mode_change_time = 0; // From original main.c

  while (1)
  {
    uint32_t now = HAL_GetTick();

    // Handle Diagnostic Stream Output (USART2)
    handle_diagnostic_stream(now); // from challenge.c

    // Handle Shell Input (LPUART1)
    uint8_t rx_char;
    HAL_StatusTypeDef rx_status = HAL_UART_Receive(&hlpuart1, &rx_char, 1, 1); // Short timeout for non-blocking feel

    if (rx_status == HAL_OK) {
        shell_process_char(rx_char, &hlpuart1); // from shell.c
    } else if (rx_status == HAL_ERROR) {
        if (hlpuart1.ErrorCode & HAL_UART_ERROR_ORE) { __HAL_UART_CLEAR_IT(&hlpuart1, UART_CLEAR_OREF); }
        if (hlpuart1.ErrorCode & HAL_UART_ERROR_NE) { __HAL_UART_CLEAR_IT(&hlpuart1, UART_CLEAR_NEF); }
        if (hlpuart1.ErrorCode & HAL_UART_ERROR_FE) { __HAL_UART_CLEAR_IT(&hlpuart1, UART_CLEAR_FEF); }
        hlpuart1.ErrorCode = HAL_UART_ERROR_NONE; // Reset error code
    }

    // Handle Capacitive Touch Input for Effect Cycling
    if (all_repairs_completed) {
        bool pressed = is_capacitive_touched(); // from utils.c
        if (pressed && !lastPressed_cap) {
            if (now - last_touch_mode_change_time >= TOUCH_MODE_CHANGE_COOLDOWN_MS) { // TOUCH_MODE_CHANGE_COOLDOWN_MS from utils.h
                
                clearAllLEDs(); // from led_control.c
                AppEffect_t previous_effect = effect;

                // Cycle through effects
                if (previous_effect == EFFECT_BREATHE)      { effect = EFFECT_CRACKLE; }
                else if (previous_effect == EFFECT_CRACKLE)   { effect = EFFECT_SCANNER; }
                else if (previous_effect == EFFECT_SCANNER)   { effect = EFFECT_CONVERGE_DIVERGE; }
                else if (previous_effect == EFFECT_CONVERGE_DIVERGE){ effect = EFFECT_ALL_ON; }
                else if (previous_effect == EFFECT_ALL_ON)    { effect = EFFECT_STRIKE; }
                else if (previous_effect == EFFECT_STRIKE)    { effect = EFFECT_OFF; }
                else if (previous_effect == EFFECT_OFF)       { effect = EFFECT_BREATHE; }
                else { effect = EFFECT_BREATHE; } // Default

                // Save the new effect if it's a valid bling mode
                if (effect != EFFECT_OFF && effect != EFFECT_STRIKE) {
                    repair_status.last_unlocked_effect = (uint8_t)effect;
                    save_repair_status(); // from challenge.c
                } else if (effect == EFFECT_OFF) {
                    if (previous_effect != EFFECT_OFF && previous_effect != EFFECT_STRIKE) {
                         repair_status.last_unlocked_effect = (uint8_t)previous_effect;
                    } else {
                         repair_status.last_unlocked_effect = (uint8_t)EFFECT_BREATHE;
                    }
                    save_repair_status();
                }
                // Don't save EFFECT_STRIKE as a persistent bling mode from touch

                if (effect == EFFECT_STRIKE) { 
                    burstActive = true;
                    // Strike state (phase, timers) reset by update_led_visuals
                    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS);
                } else if (effect == EFFECT_OFF) {
                    // Eye pulse state reset by update_led_visuals
                     __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, EYE_SOLID_ON_BRIGHTNESS);
                }
                last_touch_mode_change_time = now;
                // Consider calling print_banner_shell() here if effect change should update banner immediately
            }
        }
        lastPressed_cap = pressed;
    } else {
        lastPressed_cap = false; 
    }

    // Update LED Visuals
    update_led_visuals(now); // from led_control.c
  }
}

/**
  * @brief System Tick Configuration for SW_PWM.
  * @retval None
  */
void SysTick_Handler(void) {
  HAL_IncTick();
  update_software_pwm(); // from sw_pwm.c
}
