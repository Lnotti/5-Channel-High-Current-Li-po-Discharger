/* =============================================================================
 * main.h — LiPo Storage Discharger
 * Target : STM32F103C8T6 @ 72MHz
 * =============================================================================
 */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------------
 * HAL peripheral handles
 * Defined in main.c, referenced everywhere else via extern.
 * ---------------------------------------------------------------------------*/
extern ADC_HandleTypeDef  hadc1;
extern I2C_HandleTypeDef  hi2c1;

/* ---------------------------------------------------------------------------
 * Gate pins  (all on GPIOB)
 * PB0, PB1, PB3, PB4, PB8
 * ---------------------------------------------------------------------------*/
#define GATE_PORT           GPIOB
#define GATE1_PIN           GPIO_PIN_0
#define GATE2_PIN           GPIO_PIN_1
#define GATE3_PIN           GPIO_PIN_3
#define GATE4_PIN           GPIO_PIN_4
#define GATE5_PIN           GPIO_PIN_8

/* ---------------------------------------------------------------------------
 * Fan  (PA5)
 * ---------------------------------------------------------------------------*/
#define FAN_PORT            GPIOA
#define FAN_PIN             GPIO_PIN_5

/* ---------------------------------------------------------------------------
 * PCA9685  (I2C1 on PB6/PB7)
 * ---------------------------------------------------------------------------*/
#define PCA9685_I2C_ADDR    (0x40 << 1)   /* HAL 8-bit format */
#define PCA9685_PWM_FREQ    1000           /* Hz */

/* ---------------------------------------------------------------------------
 * Application entry point (called from main() after MX_ inits)
 * ---------------------------------------------------------------------------*/
void App_Run(void);

/* ---------------------------------------------------------------------------
 * CubeMX-style init prototypes (defined in main.c)
 * ---------------------------------------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);

#ifdef __cplusplus
}
#endif
#endif /* __MAIN_H */
