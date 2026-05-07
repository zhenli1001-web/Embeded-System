#include "main.h"
#include "gpio.h"
#include "adc.h"
#include "rng.h"
#include "tim.h"
#include "usart.h"

#include "LCD.h"
#include "Joystick.h"
#include "Buzzer.h"
#include "PWM.h"

#include "Menu.h"
#include "InputHandler.h"
#include "Game_1.h"
#include "Game_2.h"
#include "Game_3.h"

#include <stdint.h>
#include <stdio.h>

void SystemClock_Config(void);
void PeriphCommonClock_Config(void);

ST7789V2_cfg_t cfg0 = {
    .setup_done = 0,
    .spi = SPI2,
    .RST = {.port = GPIOB, .pin = GPIO_PIN_2},
    .BL = {.port = GPIOB, .pin = GPIO_PIN_1},
    .DC = {.port = GPIOB, .pin = GPIO_PIN_11},
    .CS = {.port = GPIOB, .pin = GPIO_PIN_12},
    .MOSI = {.port = GPIOB, .pin = GPIO_PIN_15},
    .SCLK = {.port = GPIOB, .pin = GPIO_PIN_13},
    .dma = {.instance = DMA1, .channel = DMA1_Channel5}
};

// ===== JOYSTICK CONFIGURATION =====
Joystick_cfg_t joystick_cfg = {
    .adc = &hadc1,
    .x_channel = ADC_CHANNEL_1, //A5 on Nucleo board
    .y_channel = ADC_CHANNEL_2, //A4 on Nucleo board
    .sampling_time = ADC_SAMPLETIME_247CYCLES_5,
    .center_x = JOYSTICK_DEFAULT_CENTER_X,
    .center_y = JOYSTICK_DEFAULT_CENTER_Y,
    .deadzone = JOYSTICK_DEADZONE,
    .setup_done = 0
};

Joystick_t joystick_data;

Buzzer_cfg_t buzzer_cfg = {
    .htim = &htim2,
    .channel = TIM_CHANNEL_3,
    .tick_freq_hz = 1000000,
    .min_freq_hz = 20,
    .max_freq_hz = 20000,
    .setup_done = 0
};

PWM_cfg_t pwm_cfg = {
    .htim = &htim4,
    .channel = TIM_CHANNEL_1,
    .tick_freq_hz = 1000000,
    .min_freq_hz = 10,
    .max_freq_hz = 50000,
    .setup_done = 0
};

volatile uint32_t g_tim6_ticks = 0;
volatile uint32_t g_tim7_ticks = 0;
volatile uint8_t g_tim6_led_flag = 0;

int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        g_tim6_ticks++;
        g_tim6_led_flag = 1;
    }

    if (htim->Instance == TIM7) {
        g_tim7_ticks++;
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    PeriphCommonClock_Config();

    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_ADC1_Init();
    MX_RNG_Init();
    MX_TIM2_Init();
    MX_TIM4_Init();
    MX_TIM6_Init();
    MX_TIM7_Init();

    LCD_init(&cfg0);
    Joystick_Init(&joystick_cfg);
    buzzer_init(&buzzer_cfg);

    PWM_Init(&pwm_cfg);
    PWM_SetFreq(&pwm_cfg, 1000);
    PWM_SetDuty(&pwm_cfg, 0);

    HAL_TIM_Base_Start_IT(&htim6);
    HAL_TIM_Base_Start_IT(&htim7);

    Input_Init();

    LCD_Fill_Buffer(0);
    LCD_Refresh(&cfg0);

    MenuSystem menu;
    Menu_Init(&menu);

    MenuState state = MENU_STATE_HOME;

    while (1)
    {
        if (state == MENU_STATE_HOME) {
            state = Menu_Run(&menu);
        }
        else if (state == MENU_STATE_GAME_1) {
            state = Game1_Run();
        }
        else if (state == MENU_STATE_GAME_2) {
            state = Game2_Run();
        }
        else if (state == MENU_STATE_GAME_3) {
            state = Game3_Run();
        }
        else {
            state = MENU_STATE_HOME;
        }
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 10;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 |
                                  RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
}

void PeriphCommonClock_Config(void)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RNG |
                                         RCC_PERIPHCLK_ADC;

    PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_PLLSAI1;
    PeriphClkInit.RngClockSelection = RCC_RNGCLKSOURCE_PLLSAI1;

    PeriphClkInit.PLLSAI1.PLLSAI1Source = RCC_PLLSOURCE_HSI;
    PeriphClkInit.PLLSAI1.PLLSAI1M = 1;
    PeriphClkInit.PLLSAI1.PLLSAI1N = 8;
    PeriphClkInit.PLLSAI1.PLLSAI1P = RCC_PLLP_DIV7;
    PeriphClkInit.PLLSAI1.PLLSAI1Q = RCC_PLLQ_DIV4;
    PeriphClkInit.PLLSAI1.PLLSAI1R = RCC_PLLR_DIV2;
    PeriphClkInit.PLLSAI1.PLLSAI1ClockOut = RCC_PLLSAI1_48M2CLK |
                                            RCC_PLLSAI1_ADC1CLK;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif