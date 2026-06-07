/* =============================================================================
 * app.c — Application Logic
 * LiPo Storage Discharger, STM32F103C8T6
 * =============================================================================
 */

#include "main.h"

/* ===========================================================================
 * Constants
 * ===========================================================================*/
#define NUM_PORTS           5
#define ADC_SAMPLES         8       /* Rolling average window               */
#define ADC_POLL_MS         150     /* How often to sample each port (ms)   */
#define FAN_COOLDOWN_MS     30000   /* Fan stays on 30s after last discharge */

#define V_STORAGE_TARGET    22.8f   /* Discharge stops here (V)             */
#define V_NO_BATTERY        18.0f   /* Below this = no battery connected    */
#define V_DIVIDER_RATIO     (115.0f / 15.0f)  /* (100k+15k)/15k             */
#define ADC_VREF            3.3f
#define ADC_RESOLUTION      4095.0f

#define BLINK_SLOW_MS       500     /* Discharging: 500ms on / 500ms off    */
#define BLINK_FAST_MS       100     /* Error:       100ms on / 100ms off    */

#define PWM_FULL            4095    /* PCA9685 full brightness              */
#define PWM_OFF             0

/* ===========================================================================
 * PCA9685 Register Map
 * ===========================================================================*/
#define PCA9685_REG_MODE1       0x00
#define PCA9685_REG_PRESCALE    0xFE
#define PCA9685_REG_LED0_ON_L   0x06

/* MODE1 bits */
#define MODE1_SLEEP     (1 << 4)
#define MODE1_AI        (1 << 5)   /* Auto-increment */
#define MODE1_ALLCALL   (1 << 0)

/* ===========================================================================
 * Port State Machine
 * ===========================================================================*/
typedef enum {
    PORT_IDLE        = 0,
    PORT_DISCHARGING = 1,
    PORT_DONE        = 2,
    PORT_ERROR       = 3
} PortState_t;

typedef struct {
    PortState_t state;
    float       voltage;
    uint32_t    adcSamples[ADC_SAMPLES];
    uint8_t     sampleIdx;
    uint32_t    lastSampleTick;
    uint32_t    blinkTick;
    bool        blinkOn;
} Port_t;

/* ===========================================================================
 * Static Data
 * ===========================================================================*/
static Port_t ports[NUM_PORTS];

/* Gate pins indexed by port (all on GPIOB) */
static const uint16_t GATE_PIN[NUM_PORTS] = {
    GPIO_PIN_0,   /* Port 1: PB0 */
    GPIO_PIN_1,   /* Port 2: PB1 */
    GPIO_PIN_3,   /* Port 3: PB3 */
    GPIO_PIN_4,   /* Port 4: PB4 */
    GPIO_PIN_8    /* Port 5: PB8 */
};

/* PCA9685 channel base per port — Red=base, Green=base+1, Blue=base+2 */
static const uint8_t LED_CH_BASE[NUM_PORTS] = {0, 3, 6, 9, 12};

/* ADC channel numbers for PA0–PA4 */
static const uint32_t ADC_CHANNEL[NUM_PORTS] = {
    ADC_CHANNEL_0,
    ADC_CHANNEL_1,
    ADC_CHANNEL_2,
    ADC_CHANNEL_3,
    ADC_CHANNEL_4
};

/* Fan state */
static uint32_t fanCooldownStart = 0;
static bool     fanCoolingDown   = false;

/* ===========================================================================
 * Private Function Prototypes
 * ===========================================================================*/
static void     Ports_Init(void);
static void     ADC_SamplePort(uint8_t i);
static float    ADC_ToVoltage(uint32_t raw);
static void     Port_UpdateState(uint8_t i);
static void     Gate_Set(uint8_t i, bool on);
static void     Fan_Update(void);
static void     LED_UpdateAll(void);
static void     LED_SetRGB(uint8_t portIdx, uint16_t r, uint16_t g, uint16_t b);
static HAL_StatusTypeDef PCA9685_WriteReg(uint8_t reg, uint8_t val);
static HAL_StatusTypeDef PCA9685_SetChannel(uint8_t ch, uint16_t on, uint16_t off);
static void     PCA9685_Init(void);

/* ===========================================================================
 * App_Run — called from main() after all MX_ inits
 * ===========================================================================*/
void App_Run(void)
{
    Ports_Init();
    PCA9685_Init();

    while (1)
    {
        for (uint8_t i = 0; i < NUM_PORTS; i++) {
            ADC_SamplePort(i);
            Port_UpdateState(i);
        }

        LED_UpdateAll();
        Fan_Update();

        HAL_Delay(10);
    }
}

/* ===========================================================================
 * Init
 * ===========================================================================*/
static void Ports_Init(void)
{
    for (uint8_t i = 0; i < NUM_PORTS; i++) {
        ports[i].state          = PORT_IDLE;
        ports[i].voltage        = 0.0f;
        ports[i].sampleIdx      = 0;
        ports[i].lastSampleTick = 0;
        ports[i].blinkTick      = 0;
        ports[i].blinkOn        = false;

        /* Initialise sample buffer to zero so averaging doesn't spike */
        for (uint8_t s = 0; s < ADC_SAMPLES; s++) {
            ports[i].adcSamples[s] = 0;
        }

        Gate_Set(i, false);
    }

    HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_RESET);
}

/* ===========================================================================
 * ADC — Voltage Sensing
 *
 * Called once per port per main loop tick.
 * Only actually converts when ADC_POLL_MS has elapsed for that port.
 * Reconfigures ADC1 channel before each conversion (scan mode disabled,
 * one channel at a time — simple, no DMA needed).
 * ===========================================================================*/
static void ADC_SamplePort(uint8_t i)
{
    uint32_t now = HAL_GetTick();
    if ((now - ports[i].lastSampleTick) < ADC_POLL_MS) return;
    ports[i].lastSampleTick = now;

    /* Reconfigure ADC1 for this port's channel */
    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel      = ADC_CHANNEL[i];
    ch.Rank         = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;   /* ~4.6µs @ 12MHz ADC clk */
    HAL_ADC_ConfigChannel(&hadc1, &ch);

    /* Single conversion */
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        uint32_t raw = HAL_ADC_GetValue(&hadc1);

        /* Store in rolling buffer */
        ports[i].adcSamples[ports[i].sampleIdx] = raw;
        ports[i].sampleIdx = (ports[i].sampleIdx + 1) % ADC_SAMPLES;

        /* Compute rolling average and convert to voltage */
        uint32_t sum = 0;
        for (uint8_t s = 0; s < ADC_SAMPLES; s++) sum += ports[i].adcSamples[s];
        ports[i].voltage = ADC_ToVoltage(sum / ADC_SAMPLES);
    }
    HAL_ADC_Stop(&hadc1);
}

static float ADC_ToVoltage(uint32_t raw)
{
    float adcV = ((float)raw / ADC_RESOLUTION) * ADC_VREF;
    return adcV * V_DIVIDER_RATIO;
}

/* ===========================================================================
 * Port State Machine
 * ===========================================================================*/
static void Port_UpdateState(uint8_t i)
{
    float v = ports[i].voltage;

    switch (ports[i].state)
    {
        case PORT_IDLE:
            if (v > V_STORAGE_TARGET) {
                Gate_Set(i, true);
                ports[i].state    = PORT_DISCHARGING;
                ports[i].blinkOn  = false;
                ports[i].blinkTick = HAL_GetTick();
            } else if (v > V_NO_BATTERY) {
                /* Battery present but already at or below storage — done */
                ports[i].state = PORT_DONE;
            }
            break;

        case PORT_DISCHARGING:
            if (v <= V_NO_BATTERY) {
                /* Battery pulled mid-discharge */
                Gate_Set(i, false);
                ports[i].state = PORT_IDLE;
            } else if (v <= V_STORAGE_TARGET) {
                Gate_Set(i, false);
                ports[i].state = PORT_DONE;
            }
            break;

        case PORT_DONE:
            if (v <= V_NO_BATTERY) {
                /* Battery removed */
                ports[i].state = PORT_IDLE;
            }
            break;

        case PORT_ERROR:
            Gate_Set(i, false);
            /* Stay in ERROR until power cycle.
             * Extend here: add a reset trigger if needed. */
            break;

        default:
            ports[i].state = PORT_ERROR;
            break;
    }
}

static void Gate_Set(uint8_t i, bool on)
{
    HAL_GPIO_WritePin(GATE_PORT, GATE_PIN[i],
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ===========================================================================
 * Fan Control
 * ===========================================================================*/
static void Fan_Update(void)
{
    bool anyDischarging = false;
    for (uint8_t i = 0; i < NUM_PORTS; i++) {
        if (ports[i].state == PORT_DISCHARGING) {
            anyDischarging = true;
            break;
        }
    }

    if (anyDischarging) {
        fanCoolingDown = false;
        HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_SET);
    } else {
        if (!fanCoolingDown) {
            fanCoolingDown   = true;
            fanCooldownStart = HAL_GetTick();
        }
        if ((HAL_GetTick() - fanCooldownStart) >= FAN_COOLDOWN_MS) {
            HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_RESET);
        }
    }
}

/* ===========================================================================
 * LED Control
 *
 * LED_UpdateAll is called every main loop iteration (~10ms).
 * Blink state is managed per-port with a simple tick comparison.
 * I2C writes happen every loop even for solid states — this is fine at
 * 100kHz I2C with only 5 ports. If I2C bandwidth becomes a concern,
 * add a dirty flag and only write on state change.
 * ===========================================================================*/
static void LED_UpdateAll(void)
{
    uint32_t now = HAL_GetTick();

    for (uint8_t i = 0; i < NUM_PORTS; i++) {
        switch (ports[i].state)
        {
            case PORT_IDLE:
                LED_SetRGB(i, PWM_OFF, PWM_OFF, PWM_OFF);
                break;

            case PORT_DISCHARGING:
                if ((now - ports[i].blinkTick) >= BLINK_SLOW_MS) {
                    ports[i].blinkTick = now;
                    ports[i].blinkOn   = !ports[i].blinkOn;
                }
                LED_SetRGB(i, PWM_OFF,
                           ports[i].blinkOn ? PWM_FULL : PWM_OFF,
                           PWM_OFF);
                break;

            case PORT_DONE:
                LED_SetRGB(i, PWM_OFF, PWM_FULL, PWM_OFF);
                break;

            case PORT_ERROR:
                if ((now - ports[i].blinkTick) >= BLINK_FAST_MS) {
                    ports[i].blinkTick = now;
                    ports[i].blinkOn   = !ports[i].blinkOn;
                }
                LED_SetRGB(i, ports[i].blinkOn ? PWM_FULL : PWM_OFF,
                           PWM_OFF, PWM_OFF);
                break;

            default:
                LED_SetRGB(i, PWM_OFF, PWM_OFF, PWM_OFF);
                break;
        }
    }
}

static void LED_SetRGB(uint8_t portIdx, uint16_t r, uint16_t g, uint16_t b)
{
    uint8_t base = LED_CH_BASE[portIdx];
    PCA9685_SetChannel(base + 0, 0, r);
    PCA9685_SetChannel(base + 1, 0, g);
    PCA9685_SetChannel(base + 2, 0, b);
}

/* ===========================================================================
 * PCA9685 Driver
 * ===========================================================================*/
static HAL_StatusTypeDef PCA9685_WriteReg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return HAL_I2C_Master_Transmit(&hi2c1, PCA9685_I2C_ADDR, buf, 2, HAL_MAX_DELAY);
}

static HAL_StatusTypeDef PCA9685_SetChannel(uint8_t ch, uint16_t on, uint16_t off)
{
    uint8_t buf[5];
    buf[0] = PCA9685_REG_LED0_ON_L + (ch * 4);
    buf[1] = (uint8_t)(on  & 0xFF);
    buf[2] = (uint8_t)(on  >> 8);
    buf[3] = (uint8_t)(off & 0xFF);
    buf[4] = (uint8_t)(off >> 8);
    return HAL_I2C_Master_Transmit(&hi2c1, PCA9685_I2C_ADDR, buf, 5, HAL_MAX_DELAY);
}

static void PCA9685_Init(void)
{
    /* Enter sleep mode — required before writing prescaler */
    PCA9685_WriteReg(PCA9685_REG_MODE1, MODE1_SLEEP);
    HAL_Delay(1);

    /* Prescaler = (25,000,000 / (4096 * freq)) - 1
     * At 1000 Hz: (25000000 / 4096000) - 1 = ~5.1 → 5 */
    uint8_t prescale = (uint8_t)((25000000.0f / (4096.0f * (float)PCA9685_PWM_FREQ)) - 1.0f);
    PCA9685_WriteReg(PCA9685_REG_PRESCALE, prescale);

    /* Wake up with auto-increment enabled */
    PCA9685_WriteReg(PCA9685_REG_MODE1, MODE1_AI | MODE1_ALLCALL);
    HAL_Delay(1);   /* Oscillator startup time */

    /* All channels off */
    for (uint8_t ch = 0; ch < 16; ch++) {
        PCA9685_SetChannel(ch, 0, 0);
    }
}
