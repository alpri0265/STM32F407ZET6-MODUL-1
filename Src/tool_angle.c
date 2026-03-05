#include "tool_angle.h"
#include "adc_if.h"
#include "board.h"
#include "stm32f4xx_hal.h"

/* Збереження в RTC Backup Registers — зберігаються при програмному скиданні (не при вимкненні живлення без VBAT). */
#define TOOL_ANGLE_MAGIC  0x5441U  /* "TA" */
#define BKP0R_OFFSET      0x50U    /* RTC->BKP0R */
#define BKP1R_OFFSET      0x54U    /* RTC->BKP1R */

static uint16_t s_ref_raw;
static float s_ref_deg;
/* Згладжування raw ADC (IIR), щоб одиниці/десяті градуса не скакали */
static uint32_t s_raw_filtered;  /* raw << 8 для дробової частини */
static uint8_t s_raw_inited;
/* Гістерезис для відображення: змінюємо повернуте значення лише при зміні >= 0.2° */
#define STABLE_HYST_DEG  0.2f
static float s_last_stable_deg = -1.0f;

static void backup_domain_enable(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    /* Тактування RTC потрібне для доступу до BKP0R/BKP1R; LSI — без зовнішнього кварцу */
    if ((RCC->BDCR & RCC_BDCR_RTCEN) == 0u) {
        __HAL_RCC_LSI_ENABLE();
        while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) == 0u) { }
        RCC->BDCR = (RCC->BDCR & ~RCC_BDCR_RTCSEL) | RCC_BDCR_RTCSEL_1; /* LSI */
        RCC->BDCR |= RCC_BDCR_RTCEN;
    }
}

static void tool_angle_persist(void)
{
    uint32_t deg_tenths = (uint32_t)(s_ref_deg * 10.0f + 0.5f);
    if (deg_tenths > 3600u) deg_tenths = 3600u;

    backup_domain_enable();
    *(__IO uint32_t *)(RTC_BASE + BKP0R_OFFSET) = ((uint32_t)TOOL_ANGLE_MAGIC << 16) | (uint32_t)s_ref_raw;
    *(__IO uint32_t *)(RTC_BASE + BKP1R_OFFSET) = deg_tenths;
}

void tool_angle_init(void)
{
    uint32_t w0, w1;

    backup_domain_enable();
    w0 = *(__IO uint32_t *)(RTC_BASE + BKP0R_OFFSET);
    w1 = *(__IO uint32_t *)(RTC_BASE + BKP1R_OFFSET);

    if ((w0 >> 16) != TOOL_ANGLE_MAGIC)
        return;
    if ((w0 & 0xFFFFu) > 4095u)
        return;
    if (w1 > 3600u)
        w1 = 3600u;

    s_ref_raw = (uint16_t)(w0 & 0xFFFFu);
    s_ref_deg = (float)w1 / 10.0f;
}

float tool_angle_get_deg(void)
{
    uint16_t raw = adc_if_read(ADC_CH_TOOL_ANGLE);
    if (!s_raw_inited) {
        s_raw_filtered = (uint32_t)raw << 8;
        s_raw_inited = 1;
    } else {
        /* Дуже сильне згладжування (1/128 нового зразка) */
        s_raw_filtered = (s_raw_filtered * 127u + ((uint32_t)raw << 8)) / 128u;
    }
    raw = (uint16_t)(s_raw_filtered >> 8);
    int32_t diff = (int32_t)raw - (int32_t)s_ref_raw;
    float deg = s_ref_deg + (float)diff * 360.0f / 4095.0f;
    while (deg < 0.0f)   deg += 360.0f;
    while (deg >= 360.0f) deg -= 360.0f;

    /* Гістерезис: оновлюємо значення для відображення лише при зміні >= 0.05° */
    {
        float rounded = (float)(int)(deg * 10.0f + 0.5f) / 10.0f;
        if (s_last_stable_deg < 0.0f) {
            s_last_stable_deg = rounded;
        } else {
            float delta = deg - s_last_stable_deg;
            if (delta > 180.0f)  delta -= 360.0f;
            if (delta < -180.0f) delta += 360.0f;
            if (delta >= -STABLE_HYST_DEG && delta <= STABLE_HYST_DEG)
                return s_last_stable_deg;
            s_last_stable_deg = rounded;
        }
        return s_last_stable_deg;
    }
}

void tool_angle_zero(void)
{
    s_ref_raw = adc_if_read(ADC_CH_TOOL_ANGLE);
    s_ref_deg = 0.0f;
    s_raw_inited = 0;
    s_last_stable_deg = -1.0f;
    tool_angle_persist();
}

void tool_angle_set_displayed_deg(float deg)
{
    s_ref_raw = adc_if_read(ADC_CH_TOOL_ANGLE);
    s_ref_deg = deg;
    s_raw_inited = 0;
    s_last_stable_deg = -1.0f;
    tool_angle_persist();
}
