#include "system_config.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* RTC Backup: BKP3R-BKP9R, BKP15R-BKP18R (tool_angle: BKP0-2, linenc: BKP10-14) */
#define SCFG_BKP_MAGIC  0x53434647UL  /* "SCFG" */
#define BKP3R_OFS       0x5CU
#define BKP4R_OFS       0x60U
#define BKP5R_OFS       0x64U
#define BKP6R_OFS       0x68U
#define BKP7R_OFS       0x6CU
#define BKP8R_OFS       0x70U
#define BKP9R_OFS       0x74U
#define BKP15R_OFS      0x8CU
#define BKP16R_OFS      0x90U
#define BKP17R_OFS      0x94U
#define BKP18R_OFS      0x98U

static void backup_domain_enable(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    if ((RCC->BDCR & RCC_BDCR_RTCEN) == 0u) {
        __HAL_RCC_LSI_ENABLE();
        while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) == 0u) { }
        RCC->BDCR = (RCC->BDCR & ~RCC_BDCR_RTCSEL) | RCC_BDCR_RTCSEL_1;
        RCC->BDCR |= RCC_BDCR_RTCEN;
    }
}

static void syscfg_nv_load(void)
{
    backup_domain_enable();
    uint32_t w3 = *(__IO uint32_t *)(RTC_BASE + BKP3R_OFS);
    if ((w3 & 0xFFFF0000UL) != (SCFG_BKP_MAGIC & 0xFFFF0000UL))
        return;  /* No valid config */
    uint32_t w4 = *(__IO uint32_t *)(RTC_BASE + BKP4R_OFS);
    uint32_t w5 = *(__IO uint32_t *)(RTC_BASE + BKP5R_OFS);
    uint32_t w6 = *(__IO uint32_t *)(RTC_BASE + BKP6R_OFS);
    uint32_t w7 = *(__IO uint32_t *)(RTC_BASE + BKP7R_OFS);
    uint32_t w8 = *(__IO uint32_t *)(RTC_BASE + BKP8R_OFS);
    uint32_t w9 = *(__IO uint32_t *)(RTC_BASE + BKP9R_OFS);
    uint32_t w15 = *(__IO uint32_t *)(RTC_BASE + BKP15R_OFS);
    uint32_t w16 = *(__IO uint32_t *)(RTC_BASE + BKP16R_OFS);
    uint32_t w17 = *(__IO uint32_t *)(RTC_BASE + BKP17R_OFS);
    uint32_t w18 = *(__IO uint32_t *)(RTC_BASE + BKP18R_OFS);

    pitch_x100[AXIS_X] = (uint16_t)(w4 & 0xFFFFu);
    if (pitch_x100[AXIS_X] < 100u) pitch_x100[AXIS_X] = 500u;
    pitch_x100[AXIS_Z] = (uint16_t)(w4 >> 16);
    if (pitch_x100[AXIS_Z] < 100u) pitch_x100[AXIS_Z] = 500u;

    microstep[AXIS_X] = (uint16_t)(w5 & 0xFFFFu);
    if (microstep[AXIS_X] < 1u) microstep[AXIS_X] = 16u;
    if (microstep[AXIS_X] > 256u) microstep[AXIS_X] = 16u;
    microstep[AXIS_Z] = (uint16_t)(w5 >> 16);
    if (microstep[AXIS_Z] < 1u) microstep[AXIS_Z] = 16u;
    if (microstep[AXIS_Z] > 256u) microstep[AXIS_Z] = 16u;

    reducer_ratio_x1000[AXIS_X] = w6;
    if (reducer_ratio_x1000[AXIS_X] < 1000u) reducer_ratio_x1000[AXIS_X] = 10000u;
    reducer_ratio_x1000[AXIS_Z] = w7;
    if (reducer_ratio_x1000[AXIS_Z] < 1000u) reducer_ratio_x1000[AXIS_Z] = 10000u;

    memcpy(&cfg[AXIS_X].max_feed, &w8, sizeof(float));
    memcpy(&cfg[AXIS_Z].max_feed, &w9, sizeof(float));
    memcpy(&cfg[AXIS_X].min_mm, &w15, sizeof(float));
    memcpy(&cfg[AXIS_Z].min_mm, &w16, sizeof(float));
    memcpy(&cfg[AXIS_X].max_mm, &w17, sizeof(float));
    memcpy(&cfg[AXIS_Z].max_mm, &w18, sizeof(float));

    apply_mech_to_axis(AXIS_X);
    apply_mech_to_axis(AXIS_Z);
}

static void syscfg_nv_save(void)
{
    backup_domain_enable();
    *(__IO uint32_t *)(RTC_BASE + BKP3R_OFS) = (SCFG_BKP_MAGIC & 0xFFFF0000UL) | 1u;
    *(__IO uint32_t *)(RTC_BASE + BKP4R_OFS) = (uint32_t)pitch_x100[AXIS_X] | ((uint32_t)pitch_x100[AXIS_Z] << 16);
    *(__IO uint32_t *)(RTC_BASE + BKP5R_OFS) = (uint32_t)microstep[AXIS_X] | ((uint32_t)microstep[AXIS_Z] << 16);
    *(__IO uint32_t *)(RTC_BASE + BKP6R_OFS) = reducer_ratio_x1000[AXIS_X];
    *(__IO uint32_t *)(RTC_BASE + BKP7R_OFS) = reducer_ratio_x1000[AXIS_Z];
    uint32_t f;
    memcpy(&f, &cfg[AXIS_X].max_feed, sizeof(float));
    *(__IO uint32_t *)(RTC_BASE + BKP8R_OFS) = f;
    memcpy(&f, &cfg[AXIS_Z].max_feed, sizeof(float));
    *(__IO uint32_t *)(RTC_BASE + BKP9R_OFS) = f;
    memcpy(&f, &cfg[AXIS_X].min_mm, sizeof(float));
    *(__IO uint32_t *)(RTC_BASE + BKP15R_OFS) = f;
    memcpy(&f, &cfg[AXIS_Z].min_mm, sizeof(float));
    *(__IO uint32_t *)(RTC_BASE + BKP16R_OFS) = f;
    memcpy(&f, &cfg[AXIS_X].max_mm, sizeof(float));
    *(__IO uint32_t *)(RTC_BASE + BKP17R_OFS) = f;
    memcpy(&f, &cfg[AXIS_Z].max_mm, sizeof(float));
    *(__IO uint32_t *)(RTC_BASE + BKP18R_OFS) = f;
}

static axis_cfg_t cfg[2];
static uint16_t pitch_x100[2];
static uint16_t microstep[2];
static uint32_t reducer_ratio_x1000[2];

/* Базові steps/rev для типового кроковика (1.8°). Якщо у вас інший motor — поміняйте тут. */
#define MOTOR_FULL_STEPS_PER_REV 200u

static uint32_t motor_steps_per_rev_from_microstep(axis_id_t a)
{
    return (uint32_t)MOTOR_FULL_STEPS_PER_REV * (uint32_t)microstep[a];
}

static void apply_mech_to_axis(axis_id_t a)
{
    float pitch_mm = (pitch_x100[a] > 0u) ? ((float)pitch_x100[a] / 100.0f) : 1.0f;
    float ratio = (reducer_ratio_x1000[a] > 0u) ? ((float)reducer_ratio_x1000[a] / 1000.0f) : 0.0f;
    float spmm = (pitch_mm > 0.0f) ? ((float)motor_steps_per_rev_from_microstep(a) * ratio / pitch_mm) : 0.0f;
    if (spmm < 0.0001f) spmm = 0.0001f;
    cfg[a].steps_per_mm = spmm;
}

void system_config_init(void)
{
    /* Defaults keep current firmware behavior (steps_per_mm ~= 400). */
    pitch_x100[AXIS_X] = 500u;          /* 5.00 mm */
    microstep[AXIS_X] = 16u;          /* 16 microsteps */
    reducer_ratio_x1000[AXIS_X] = 10000u; /* 10:1 */
    cfg[AXIS_X] = (axis_cfg_t){0.0f, 3000.0f, -200.0f, 0.0f};
    apply_mech_to_axis(AXIS_X);

    pitch_x100[AXIS_Z] = 500u;          /* 5.00 mm */
    microstep[AXIS_Z] = 16u;          /* 16 microsteps */
    reducer_ratio_x1000[AXIS_Z] = 10000u; /* 10:1 */
    cfg[AXIS_Z] = (axis_cfg_t){0.0f, 2500.0f, -500.0f, 0.0f};
    apply_mech_to_axis(AXIS_Z);

    syscfg_nv_load();  /* Перезаписати з RTC backup, якщо є валідні дані */
}

void system_config_save(void)
{
    syscfg_nv_save();
}

const axis_cfg_t* system_axis_cfg(axis_id_t a)
{
    return &cfg[a];
}

void system_axis_set_max_feed(axis_id_t axis, float v)
{
    if (v < 100.0f) v = 100.0f;
    if (v > 20000.0f) v = 20000.0f;
    cfg[axis].max_feed = v;
}

void system_axis_set_min_mm(axis_id_t axis, float v)
{
    if (v < -2000.0f) v = -2000.0f;
    if (v > 2000.0f) v = 2000.0f;
    cfg[axis].min_mm = v;
}

void system_axis_set_max_mm(axis_id_t axis, float v)
{
    if (v < -2000.0f) v = -2000.0f;
    if (v > 2000.0f) v = 2000.0f;
    cfg[axis].max_mm = v;
}

uint16_t system_mech_get_pitch_x100(axis_id_t axis)
{
    return pitch_x100[axis];
}

void system_mech_set_pitch_x100(axis_id_t axis, uint16_t pitch)
{
    /* Мінімум 1.00 мм щоб уникнути ділення на нуль. */
    if (pitch < 100u) pitch = 100u;
    pitch_x100[axis] = pitch;
    apply_mech_to_axis(axis);
}

uint32_t system_mech_get_motor_steps_per_rev(axis_id_t axis)
{
    return motor_steps_per_rev_from_microstep(axis);
}

void system_mech_set_motor_steps_per_rev(axis_id_t axis, uint32_t steps_per_rev)
{
    /* Переводимо steps/rev у microstep відносно базових MOTOR_FULL_STEPS_PER_REV. */
    if (steps_per_rev < 1u) steps_per_rev = 1u;
    uint32_t ms = (steps_per_rev + (MOTOR_FULL_STEPS_PER_REV / 2u)) / MOTOR_FULL_STEPS_PER_REV;
    if (ms < 1u) ms = 1u;
    if (ms > 65535u) ms = 65535u;
    microstep[axis] = (uint16_t)ms;
    apply_mech_to_axis(axis);
}

uint16_t system_mech_get_microstep(axis_id_t axis)
{
    return microstep[axis];
}

void system_mech_set_microstep(axis_id_t axis, uint16_t ms)
{
    /* Обмежимо, щоб не було дивних значень. */
    if (ms < 1u) ms = 1u;
    if (ms > 256u) ms = 256u;
    microstep[axis] = ms;
    apply_mech_to_axis(axis);
}

uint32_t system_mech_get_reducer_ratio_x1000(axis_id_t axis)
{
    return reducer_ratio_x1000[axis];
}

void system_mech_set_reducer_ratio_x1000(axis_id_t axis, uint32_t ratio_x1000_v)
{
    if (ratio_x1000_v < 1u) ratio_x1000_v = 1u;
    if (ratio_x1000_v > 1000000u) ratio_x1000_v = 1000000u;
    reducer_ratio_x1000[axis] = ratio_x1000_v;
    apply_mech_to_axis(axis);
}
