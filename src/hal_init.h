#ifndef HAL_INIT_H
#define HAL_INIT_H

#include "stm32l0xx_hal.h"

/* HAL Handle Declarations (to be defined in main.c or hal_init.c) */
extern UART_HandleTypeDef hlpuart1;
extern UART_HandleTypeDef huart2;
extern ADC_HandleTypeDef hadc;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim21;
extern TIM_HandleTypeDef htim22;

/* Function Prototypes */
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_LPUART1_UART_Init(void);
void MX_USART2_UART_Init(void);
void MX_ADC_Init(void);
void MX_TIM2_Init(void);
void MX_TIM21_Init(void);
void MX_TIM22_Init(void);

// MSP Functions are typically called by HAL_Init functions,
// but their prototypes can be here for completeness if needed elsewhere,
// or they can remain static in hal_init.c if only used there.
// For now, let's assume they are primarily used by HAL_Init within hal_init.c
// void HAL_UART_MspInit(UART_HandleTypeDef *huart);
// void HAL_UART_MspDeInit(UART_HandleTypeDef* huart);
// void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef* htim_pwm);
// void HAL_TIM_PWM_MspDeInit(TIM_HandleTypeDef* htim_pwm);
// void HAL_ADC_MspInit(ADC_HandleTypeDef* adcHandle);
// void HAL_ADC_MspDeInit(ADC_HandleTypeDef* adcHandle);

#endif // HAL_INIT_H
