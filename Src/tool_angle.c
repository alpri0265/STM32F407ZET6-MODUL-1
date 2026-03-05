#include "tool_angle.h"
#include "adc_if.h"
#include "board.h"
#include "stm32f4xx_hal.h"

/* Збереження в RTC Backup Registers — зберігаються при програмному скиданні (не при вимкненні живлення без VBAT). */
#define TOOL_ANGLE_MAGIC  0x5441U  /* "TA" */
#define BKP0R_OFFSET      0x50U    /* RTC->BKP0R */
#define BKP1R_OFFSET      0x54U    /* RTC->BKP1R */
#define BKP2R_OFFSET      0x58U    /* RTC->BKP2R — коефіцієнт калібрування K*10000 */

static uint16_t s_ref_raw;
static float s_ref_deg;
/* Коефіцієнт калібрування: deg = ref_deg + diff*360/4095*K; 1.0f = без калібрування */
static float s_calib_k = 1.0f;
/* Згладжування raw ADC (IIR), щоб одиниці/десяті градуса не скакали */
static uint32_t s_raw_filtered;  /* raw << 8 для дробової частини */
static uint8_t s_raw_inited;
/* Гістерезис для відображення: змінюємо повернуте значення лише при зміні >= 0.2° */
#define STABLE_HYST_DEG  0.2f
/* Зона нуля: кути в [0, 1.0) або [358.0, 360) показуємо як 0.0 (усуває 358.8–0.3 після Zero) */
#define ZERO_ZONE_HIGH   1.0f
#define ZERO_ZONE_WRAP   358.0f
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
    uint32_t k_x10000 = (uint32_t)(s_calib_k * 10000.0f + 0.5f);
    if (k_x10000 < 5000u) k_x10000 = 5000u;   /* min 0.5 */
    if (k_x10000 > 15000u) k_x10000 = 15000u; /* max 1.5 */

    backup_domain_enable();
    *(__IO uint32_t *)(RTC_BASE + BKP0R_OFFSET) = ((uint32_t)TOOL_ANGLE_MAGIC << 16) | (uint32_t)s_ref_raw;
    *(__IO uint32_t *)(RTC_BASE + BKP1R_OFFSET) = deg_tenths;
    *(__IO uint32_t *)(RTC_BASE + BKP2R_OFFSET) = k_x10000;
}

void tool_angle_init(void)
{
    uint32_t w0, w1, w2;

    backup_domain_enable();
    w0 = *(__IO uint32_t *)(RTC_BASE + BKP0R_OFFSET);
    w1 = *(__IO uint32_t *)(RTC_BASE + BKP1R_OFFSET);
    w2 = *(__IO uint32_t *)(RTC_BASE + BKP2R_OFFSET);

    if ((w0 >> 16) != TOOL_ANGLE_MAGIC)
        return;
    if ((w0 & 0xFFFFu) > 4095u)
        return;
    if (w1 > 3600u)
        w1 = 3600u;

    s_ref_raw = (uint16_t)(w0 & 0xFFFFu);
    s_ref_deg = (float)w1 / 10.0f;
    if (w2 != 0u && w2 != 0xFFFFFFFFu && w2 >= 5000u && w2 <= 15000u)
        s_calib_k = (float)w2 / 10000.0f;
}

float tool_angle_get_deg(void)
{
    uint16_t raw = adc_if_read(ADC_CH_TOOL_ANGLE);
    if (!s_raw_inited) {
        s_raw_filtered = (uint32_t)raw << 8;
        s_raw_inited = 1;
    } else {
        /* Помірне згладжування (1/8 нового зразка) — швидший відгук при обертанні енкодера */
        s_raw_filtered = (s_raw_filtered * 7u + ((uint32_t)raw << 8)) / 8u;
    }
    raw = (uint16_t)(s_raw_filtered >> 8);
    int32_t diff = (int32_t)raw - (int32_t)s_ref_raw;
    float deg = s_ref_deg + (float)diff * 360.0f / 4095.0f * s_calib_k;
    while (deg < 0.0f)   deg += 360.0f;
    while (deg >= 360.0f) deg -= 360.0f;

    /* Зона нуля: біля 0° або 360° показуємо чистий нуль */
    if (deg < ZERO_ZONE_HIGH || deg >= ZERO_ZONE_WRAP) {
        s_last_stable_deg = 0.0f;
        return 0.0f;
    }

    /* Гістерезис: оновлюємо значення для відображення лише при зміні >= 0.2° */
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

void tool_angle_calibrate_180(void)
{
    /* Оновлюємо фільтр кількома викликами, щоб поточний raw був стабільний */
    for (int i = 0; i < 8; i++)
        (void)tool_angle_get_deg();
    uint16_t raw = (uint16_t)(s_raw_filtered >> 8);
    int32_t diff = (int32_t)raw - (int32_t)s_ref_raw;
    int32_t adiff = diff >= 0 ? diff : -diff;
    if (adiff < 50)
        return; /* Замала різниця — пропускаємо, щоб уникнути ділення на мале число */
    /* 180° = ref_deg + diff * 360/4095 * K  =>  K = 180 / (diff * 360/4095) = 180*4095/(360*diff) */
    float k = 180.0f * 4095.0f / (360.0f * (float)adiff);
    if (k < 0.5f) k = 0.5f;
    if (k > 1.5f) k = 1.5f;
    s_calib_k = k;
    tool_angle_persist();
}
