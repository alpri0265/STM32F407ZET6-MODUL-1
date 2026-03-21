/**
 * @file touch.c
 * @brief XPT2046 resistive touch driver (SPI)
 *
 * Спільні SCK (PE7) та MOSI (PE9) з дисплеєм ILI9341.
 * Окремі: T_CS (PE14), T_DO/MISO (PE15), T_IRQ (PE6, опційно).
 */
#include "touch.h"
#include "tft_config.h"
#include "stm32f4xx_hal.h"

/* XPT2046 команди (single-ended) */
#define XPT2046_CMD_X  0x90u
#define XPT2046_CMD_Y  0xD0u

/* Без дотику XPT2046 повертає (0, 4095). Валідний діапазон при натисканні ~200–3900. */
#define XPT2046_MIN_VALID  100
#define XPT2046_MAX_VALID  3950

static uint8_t touch_spi_read_byte(uint8_t out)
{
    uint8_t res = 0;
    /* TFT_CS високий — дисплей не слухає. TOUCH_CS активує тач. */
    HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_SET);   /* дисплей вимкнено */
    HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_RESET);
    for (int i = 7; i >= 0; i--) {
        HAL_GPIO_WritePin(TFT_SCK_PORT, TFT_SCK_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(TFT_MOSI_PORT, TFT_MOSI_PIN, (out & (1u << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        for (volatile int d = 0; d < 2; d++) (void)d;
        HAL_GPIO_WritePin(TFT_SCK_PORT, TFT_SCK_PIN, GPIO_PIN_SET);
        if (HAL_GPIO_ReadPin(TOUCH_MISO_PORT, TOUCH_MISO_PIN) == GPIO_PIN_SET)
            res |= (1u << i);  /* MSB-first: перший отриманий біт → старший біт */
        for (volatile int d = 0; d < 2; d++) (void)d;
    }
    HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_SET);
    return res;
}

static uint16_t touch_read_raw(uint8_t cmd)
{
    (void)touch_spi_read_byte(cmd);  /* команда, результат ігноруємо */
    uint8_t hi = touch_spi_read_byte(0u);
    uint8_t lo = touch_spi_read_byte(0u);
    /* XPT2046: 12-бітний результат у старших бітах, зсув вправо на 3 */
    return (uint16_t)(((uint16_t)hi << 8) | lo) >> 3;
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
    g.Pull = GPIO_NOPULL;
    g.Pin = TOUCH_MISO_PIN;
    HAL_GPIO_Init(TOUCH_MISO_PORT, &g);

    g.Pin = TOUCH_IRQ_PIN;
    HAL_GPIO_Init(TOUCH_IRQ_PORT, &g);

    HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_SET);
}

bool touch_read(touch_point_t *p)
{
    if (!p) return false;

    uint16_t rx = touch_read_raw(XPT2046_CMD_X);
    uint16_t ry = touch_read_raw(XPT2046_CMD_Y);

    /* Без дотику: (0, 4095). Валідний діапазон при натисканні */
    if (rx < XPT2046_MIN_VALID || rx > XPT2046_MAX_VALID ||
        ry < XPT2046_MIN_VALID || ry > XPT2046_MAX_VALID) {
        p->pressed = false;
        return false;
    }

    /* Мапавання raw ADC → екран 320x240 (ландшафт) */
    int32_t x = (int32_t)(rx - TOUCH_X_MIN) * TFT_WIDTH  / (TOUCH_X_MAX - TOUCH_X_MIN);
    int32_t y = (int32_t)(ry - TOUCH_Y_MIN) * TFT_HEIGHT / (TOUCH_Y_MAX - TOUCH_Y_MIN);

#if defined(TOUCH_SWAP_XY) && (TOUCH_SWAP_XY)
    { int32_t t = x; x = y; y = t; }
#endif
#if defined(TOUCH_INVERT_X) && (TOUCH_INVERT_X)
    x = (int32_t)TFT_WIDTH - 1 - x;
#endif
#if defined(TOUCH_INVERT_Y) && (TOUCH_INVERT_Y)
    y = (int32_t)TFT_HEIGHT - 1 - y;
#endif

    if (x < 0) x = 0;
    if (x >= (int32_t)TFT_WIDTH)  x = (int32_t)TFT_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= (int32_t)TFT_HEIGHT) y = (int32_t)TFT_HEIGHT - 1;

    p->x = (uint16_t)x;
    p->y = (uint16_t)y;
    p->pressed = true;
    return true;
}
