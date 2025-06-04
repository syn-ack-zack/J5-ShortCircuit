#include "hal_init.h"
#include "utils.h" // For CAP_PAD_PIN and CAP_PAD_PORT
#include "stm32l0xx_hal_adc.h" // Explicit include for ADC defines

/* HAL Handle Definitions (if not in main.c) */
// If these are to be global and accessible, they should be defined here or in main.c.
// The plan was to define them in main.c and extern them here.
// For now, this file will contain the init functions that configure these handles.

void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { while(1); /* Error_Handler(); */ }
  
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) { while(1); /* Error_Handler(); */ }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_LPUART1;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    while(1); /* Error_Handler(); */
  }
}

void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE(); 

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_5, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_3, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = CAP_PAD_PIN; // Defined in utils.h, but MX_GPIO_Init is here
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(CAP_PAD_PORT, &GPIO_InitStruct); // CAP_PAD_PORT defined in utils.h

  // Configure PA2 (LPUART1_TX) and PA3 (LPUART1_RX)
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF6_LPUART1; // LPUART1 AF
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // Configure PA9 (USART2_TX) and PA10 (USART2_RX)
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_USART2; // USART2 AF
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void MX_LPUART1_UART_Init(void) {
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 115200;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK) {
    while(1); /* Error_Handler(); */
  }
}

void MX_USART2_UART_Init(void) {
  huart2.Instance          = USART2;
  huart2.Init.BaudRate     = 9600;
  huart2.Init.WordLength   = UART_WORDLENGTH_8B;
  huart2.Init.StopBits     = UART_STOPBITS_1;
  huart2.Init.Parity       = UART_PARITY_NONE;
  huart2.Init.Mode         = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK) while (1); /* Error_Handler(); */
}

void MX_ADC_Init(void) {
  ADC_ChannelConfTypeDef sConfig = {0}; 
  hadc.Instance = ADC1;
  hadc.Init.OversamplingMode = DISABLE; 
  hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc.Init.Resolution = ADC_RESOLUTION_12B; 
  hadc.Init.SamplingTime = ADC_SAMPLETIME_160CYCLES_5;
  hadc.Init.ScanConvMode = 0U; // Reverted to original value
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT; 
  hadc.Init.ContinuousConvMode = DISABLE; 
  hadc.Init.DiscontinuousConvMode = DISABLE;
  hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE; 
  hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START; 
  hadc.Init.DMAContinuousRequests = DISABLE;
  hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV; 
  hadc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc.Init.LowPowerAutoWait = DISABLE; 
  hadc.Init.LowPowerAutoPowerOff = DISABLE;
  if (HAL_ADC_Init(&hadc) != HAL_OK) { while(1); /* Error_Handler(); */ }
  
  sConfig.Channel = ADC_CHANNEL_VREFINT; 
  sConfig.Rank = 1U; // Reverted to original value
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK) { while(1); /* Error_Handler(); */ }
  
  if (HAL_ADCEx_Calibration_Start(&hadc, ADC_SINGLE_ENDED) != HAL_OK) { while(1); /* Error_Handler(); */ }
}

void MX_TIM2_Init(void) {
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  uint32_t PWM_Prescaler = 61; // Consider making this a #define if used elsewhere or for clarity
  
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = PWM_Prescaler;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 255;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) { while(1); /* Error_Handler(); */ }
  
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) { while(1); /* Error_Handler(); */ }
  
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) { while(1); /* Error_Handler(); */ }
  
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

  sConfigOC.Pulse = 0;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) { while(1); /* Error_Handler(); */ }

  sConfigOC.Pulse = 0;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) { while(1); /* Error_Handler(); */ }

  HAL_TIM_GenerateEvent(&htim2, TIM_EVENTSOURCE_UPDATE);
}

void MX_TIM21_Init(void) {
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  // TIM_OC_InitTypeDef sConfigOC = {0}; // sConfigOC not used in original TIM21_Init for PWM
  uint32_t PWM_Prescaler = 61; 
  
  htim21.Instance = TIM21;
  htim21.Init.Prescaler = PWM_Prescaler;
  htim21.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim21.Init.Period = 255;
  htim21.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim21.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  // Original code initializes TIM21 as PWM but doesn't configure channels here.
  // Assuming it's a base timer init or PWM channels configured elsewhere/later.
  // If it's for PWM, HAL_TIM_PWM_Init should be used. For basic timer, HAL_TIM_Base_Init.
  // Sticking to original structure:
  if (HAL_TIM_PWM_Init(&htim21) != HAL_OK) { while(1); /* Error_Handler(); */ } // Or HAL_TIM_Base_Init if not PWM
  
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim21, &sClockSourceConfig) != HAL_OK) { while(1); /* Error_Handler(); */ }
  
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim21, &sMasterConfig) != HAL_OK) { while(1); /* Error_Handler(); */ }
  
  HAL_TIM_GenerateEvent(&htim21, TIM_EVENTSOURCE_UPDATE);
}

void MX_TIM22_Init(void) {
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  uint32_t PWM_Prescaler = 61;
  
  htim22.Instance = TIM22;
  htim22.Init.Prescaler = PWM_Prescaler;
  htim22.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim22.Init.Period = 255;
  htim22.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim22.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim22) != HAL_OK) { while(1); /* Error_Handler(); */ }
  
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim22, &sClockSourceConfig) != HAL_OK) { while(1); /* Error_Handler(); */ }
  
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim22, &sMasterConfig) != HAL_OK) { while(1); /* Error_Handler(); */ }
  
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim22, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) { while(1); /* Error_Handler(); */ }
  
  HAL_TIM_GenerateEvent(&htim22, TIM_EVENTSOURCE_UPDATE);
}

/* MSP Initialization and De-Initialization Functions */
void HAL_UART_MspInit(UART_HandleTypeDef *huart) {
  // GPIO_InitTypeDef GPIO_InitStruct = {0}; // Removed unused variable
  if (huart->Instance == LPUART1) {
    __HAL_RCC_LPUART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE(); 
    // GPIOs for LPUART1 are configured in MX_GPIO_Init()
  } else if (huart->Instance == USART2) {
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE(); 
    // GPIOs for USART2 are configured in MX_GPIO_Init()
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* huart) {
  if(huart->Instance==LPUART1) {
    __HAL_RCC_LPUART1_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2|GPIO_PIN_3);
  } else if(huart->Instance==USART2) {
    __HAL_RCC_USART2_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9|GPIO_PIN_10);
  }
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef* htim_pwm) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(htim_pwm->Instance==TIM2) {
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    // __HAL_RCC_GPIOB_CLK_ENABLE(); // GPIOB not used for TIM2 CH1/2 in original

    GPIO_InitStruct.Pin = GPIO_PIN_0; // PA0 for TIM2_CH1
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; GPIO_InitStruct.Pull = GPIO_NOPULL; GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; GPIO_InitStruct.Alternate = GPIO_AF2_TIM2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_1; // PA1 for TIM2_CH2
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; GPIO_InitStruct.Pull = GPIO_NOPULL; GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; GPIO_InitStruct.Alternate = GPIO_AF2_TIM2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  }
  else if(htim_pwm->Instance==TIM21) {
    __HAL_RCC_TIM21_CLK_ENABLE();
    // GPIOs for TIM21 PWM channels would be configured here if used
  }
  else if(htim_pwm->Instance==TIM22) {
    __HAL_RCC_TIM22_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_6; // PA6 for TIM22_CH1
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; GPIO_InitStruct.Pull = GPIO_NOPULL; GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; GPIO_InitStruct.Alternate = GPIO_AF5_TIM22;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
}

void HAL_TIM_PWM_MspDeInit(TIM_HandleTypeDef* htim_pwm) {
  if(htim_pwm->Instance==TIM2) {
    __HAL_RCC_TIM2_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0 | GPIO_PIN_1);
  }
  else if(htim_pwm->Instance==TIM21) {
    __HAL_RCC_TIM21_CLK_DISABLE();
    // HAL_GPIO_DeInit for TIM21 pins if configured
  }
  else if(htim_pwm->Instance==TIM22) {
    __HAL_RCC_TIM22_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_6);
  }
}

void HAL_ADC_MspInit(ADC_HandleTypeDef* adcHandle) {
  if(adcHandle->Instance==ADC1) { 
    __HAL_RCC_ADC1_CLK_ENABLE(); 
    __HAL_RCC_SYSCFG_CLK_ENABLE(); 
    HAL_ADCEx_EnableVREFINT(); 
  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef* adcHandle) {
  if(adcHandle->Instance==ADC1) { 
    __HAL_RCC_ADC1_CLK_DISABLE(); 
    HAL_ADCEx_DisableVREFINT(); 
    // GPIO DeInit for ADC pins if they were configured in MspInit (not the case here)
  }
}
