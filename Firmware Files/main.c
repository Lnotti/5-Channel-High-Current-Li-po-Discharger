/* =============================================================================
 * main.c — LiPo Storage Discharger
 * Target : STM32F103C8T6 @ 72MHz
 * Toolchain: STM32CubeIDE + HAL
 *
 * CubeMX peripheral configuration summary:
 *   ADC1  — Scan mode, 5 channels (IN0–IN4 = PA0–PA4), software trigger,
 *            continuous conversion OFF (we trigger manually per port)
 *   I2C1  — Standard mode 100kHz, PB6=SCL, PB7=SDA
 *   GPIO  — PB0/1/3/4/8 push-pull outputs (gates)
 *            PA5 push-pull output (fan)
 *            PA11/PA12 USB (handled by bootloader, no init needed in app)
 * =============================================================================
 */

#include "main.h"

/* ---------------------------------------------------------------------------
 * HAL handles — defined here, extern'd in main.h
 * ---------------------------------------------------------------------------*/
ADC_HandleTypeDef  hadc1;
I2C_HandleTypeDef  hi2c1;

/* ===========================================================================
 * main()
 * ===========================================================================*/
int main(void)
{
    HAL_Init();
    SystemClock_Config();   /* 72 MHz from HSE via PLL */

    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_I2C1_Init();

    /* Run ADC self-calibration — mandatory on F1 for accurate readings */
    HAL_ADCEx_Calibration_Start(&hadc1);

    App_Run();   /* Never returns */
}

/* ===========================================================================
 * SystemClock_Config
 * 72 MHz: HSE 8MHz → PLL x9
 * AHB/APB2 = 72MHz, APB1 = 36MHz
 * ADC clock = PCLK2/6 = 12MHz  (max 14MHz)
 * ===========================================================================*/
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    RCC_PeriphCLKInitTypeDef periph = {0};

    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState            = RCC_HSE_ON;
    osc.HSEPredivValue      = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL          = RCC_PLL_MUL9;
    HAL_RCC_OscConfig(&osc);

    clk.ClockType           = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                            | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource        = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider       = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider      = RCC_HCLK_DIV2;   /* 36 MHz max for APB1 */
    clk.APB2CLKDivider      = RCC_HCLK_DIV1;   /* 72 MHz */
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);

    periph.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    periph.AdcClockSelection    = RCC_ADCPCLK2_DIV6;   /* 12 MHz */
    HAL_RCCEx_PeriphCLKConfig(&periph);
}

/* ===========================================================================
 * MX_GPIO_Init
 * Gate outputs: PB0, PB1, PB3, PB4, PB8 — push-pull, default LOW
 * Fan output:   PA5                       — push-pull, default LOW
 * ADC inputs:   PA0–PA4 are configured as analog by MX_ADC1_Init
 * ===========================================================================*/
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* --- Gate outputs on GPIOB --- */
    HAL_GPIO_WritePin(GATE_PORT,
                      GATE1_PIN | GATE2_PIN | GATE3_PIN | GATE4_PIN | GATE5_PIN,
                      GPIO_PIN_RESET);

    gpio.Pin   = GATE1_PIN | GATE2_PIN | GATE3_PIN | GATE4_PIN | GATE5_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GATE_PORT, &gpio);

    /* --- Fan output on GPIOA --- */
    HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_RESET);

    gpio.Pin   = FAN_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(FAN_PORT, &gpio);
}

/* ===========================================================================
 * MX_ADC1_Init
 * Scan mode OFF — we convert one channel at a time per port.
 * Channels IN0–IN4 (PA0–PA4).
 * We reconfigure the channel rank before each conversion in app.c.
 * ===========================================================================*/
static void MX_ADC1_Init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Configure PA0–PA4 as analog inputs */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4;
    gpio.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &gpio);

    hadc1.Instance                   = ADC1;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;  /* Single channel per call */
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    HAL_ADC_Init(&hadc1);
}

/* ===========================================================================
 * MX_I2C1_Init
 * Standard mode, 100 kHz, PB6=SCL, PB7=SDA
 * ===========================================================================*/
static void MX_I2C1_Init(void)
{
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB6 SCL, PB7 SDA — alternate function open-drain */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode  = GPIO_MODE_AF_OD;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
}

/* ===========================================================================
 * HAL_MspInit — called by HAL_Init()
 * ===========================================================================*/
void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    /* Disable JTAG, keep SWD — frees PB3, PB4 for GPIO use as gate pins */
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}
