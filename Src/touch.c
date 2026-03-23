/**
 * @file touch.c
 * @brief XPT2046 resistive touch driver (SPI)
 *
 * Спільні SCK (PE7) та MOSI (PE9) з дисплеєм ILI9341.
 * T_CS (PE14), T_DO/MISO (PE15). T_IRQ не підключати — PE6 на платі = ENC_STEP_S2.
 *
 * Для стабільності: кілька знімань + медіана (XPT2046 дає шум), сповільнений SPI.
 */
#include "touch.h"
#include "tft_config.h"
#include "stm32f4xx_hal.h"

#define XPT2046_CMD_X  0x90u
#define XPT2046_CMD_Y  0xD0u
#define XPT2046_CMD_Z1 0xB8u

#ifndef TOUCH_SPI_NOP_PER_HALF
#define TOUCH_SPI_NOP_PER_HALF  120u
#endif

#ifndef TOUCH_SAMPLES
#define TOUCH_SAMPLES  3u /* медіана з 3 знімань */
#endif

/* Пауза між зніманнями одного каналу (µs-подібно) — панель «встигає» */
#ifndef TOUCH_INTER_SAMPLE_DELAY
#define TOUCH_INTER_SAMPLE_DELAY  800u
#endif

#define XPT2046_MIN_VALID  10
#define XPT2046_MAX_VALID  4090

static void touch_spi_delay_halfbit(void)
{
    for (volatile uint32_t n = 0; n < TOUCH_SPI_NOP_PER_HALF; n++)
        __NOP();
}

static void touch_inter_sample_delay(void)
{
    for (volatile uint32_t n = 0; n < TOUCH_INTER_SAMPLE_DELAY; n++)
        __NOP();
}

static uint8_t touch_spi_xchg(uint8_t out)
{
    uint8_t res = 0;
    for (int i = 7; i >= 0; i--) {
        HAL_GPIO_WritePin(TFT_SCK_PORT, TFT_SCK_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(TFT_MOSI_PORT, TFT_MOSI_PIN, (out & (1u << (unsigned)i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        touch_spi_delay_halfbit();
        HAL_GPIO_WritePin(TFT_SCK_PORT, TFT_SCK_PIN, GPIO_PIN_SET);
        touch_spi_delay_halfbit();
        if (HAL_GPIO_ReadPin(TOUCH_MISO_PORT, TOUCH_MISO_PIN) == GPIO_PIN_SET)
            res |= (1u << (unsigned)i);
    }
    return res;
}

static uint16_t touch_read_raw_once(uint8_t cmd)
{
    HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_RESET);
    for (volatile uint32_t w = 0; w < 200u; w++)
        __NOP();

    (void)touch_spi_xchg(cmd);
    uint8_t hi = touch_spi_xchg(0u);
    uint8_t lo = touch_spi_xchg(0u);
    HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_SET);
    return (uint16_t)(((uint16_t)hi << 8) | lo) >> 3;
}

static uint16_t median3(uint16_t a, uint16_t b, uint16_t c)
{
    uint16_t x = a, y = b, z = c;
    if (x > y) {
        uint16_t t = x;
        x = y;
        y = t;
    }
    if (y > z) {
        uint16_t t = y;
        y = z;
        z = t;
    }
    if (x > y) {
        uint16_t t = x;
        x = y;
        y = t;
    }
    return y;
}

static uint16_t touch_read_raw_median(uint8_t cmd)
{
    uint16_t s0 = touch_read_raw_once(cmd);
    touch_inter_sample_delay();
    uint16_t s1 = touch_read_raw_once(cmd);
    touch_inter_sample_delay();
    uint16_t s2 = touch_read_raw_once(cmd);
    return median3(s0, s1, s2);
}

void touch_init(void)
{
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Pin = TOUCH_CS_PIN;
    HAL_GPIO_Init(TOUCH_CS_PORT, &g);

    g.Mode = GPIO_MODE_INPUT;
#if TOUCH_MISO_USE_PULLUP
    g.Pull = GPIO_PULLUP;
#else
    g.Pull = GPIO_NOPULL;
#endif
    g.Pin = TOUCH_MISO_PIN;
    HAL_GPIO_Init(TOUCH_MISO_PORT, &g);

    HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_SET);
}

bool touch_read(touch_point_t *p)
{
    if (!p)
        return false;

    p->raw_x = 0;
    p->raw_y = 0;

#if defined(TOUCH_SKIP_Z1) && (TOUCH_SKIP_Z1)
#else
    uint16_t z1 = touch_read_raw_median(XPT2046_CMD_Z1);
    if (z1 > TOUCH_Z1_PRESS_MAX || z1 < TOUCH_Z1_PRESS_MIN) {
        p->pressed = false;
        return false;
    }
#endif

    uint16_t rx = touch_read_raw_median(XPT2046_CMD_X);
    touch_inter_sample_delay();
    uint16_t ry = touch_read_raw_median(XPT2046_CMD_Y);

    p->raw_x = rx;
    p->raw_y = ry;

    if ((rx < 40u && ry > 4000u) || (ry < 40u && rx > 4000u)) {
        p->pressed = false;
        return false;
    }

    if (rx < XPT2046_MIN_VALID || rx > XPT2046_MAX_VALID ||
        ry < XPT2046_MIN_VALID || ry > XPT2046_MAX_VALID) {
        p->pressed = false;
        return false;
    }

    int32_t x = (int32_t)(rx - TOUCH_X_MIN) * TFT_WIDTH / (int32_t)(TOUCH_X_MAX - TOUCH_X_MIN);
    int32_t y = (int32_t)(ry - TOUCH_Y_MIN) * TFT_HEIGHT / (int32_t)(TOUCH_Y_MAX - TOUCH_Y_MIN);

#if defined(TOUCH_SWAP_XY) && (TOUCH_SWAP_XY)
    {
        int32_t t = x;
        x = y;
        y = t;
    }
#endif
#if defined(TOUCH_INVERT_X) && (TOUCH_INVERT_X)
    x = (int32_t)TFT_WIDTH - 1 - x;
#endif
#if defined(TOUCH_INVERT_Y) && (TOUCH_INVERT_Y)
    y = (int32_t)TFT_HEIGHT - 1 - y;
#endif

    y += (int32_t)TOUCH_Y_PX_OFFSET;

    if (x < 0)
        x = 0;
    if (x >= (int32_t)TFT_WIDTH)
        x = (int32_t)TFT_WIDTH - 1;
    if (y < 0)
        y = 0;
    if (y >= (int32_t)TFT_HEIGHT)
        y = (int32_t)TFT_HEIGHT - 1;

    p->x = (uint16_t)x;
    p->y = (uint16_t)y;
    p->pressed = true;
    return true;
}
