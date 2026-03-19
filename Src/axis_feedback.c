#include "axis_feedback.h"
#include "main.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"
#include <stdint.h>

/*
 * Лінійні енкодери: TIM2 (X), TIM3 (Z). 1 крок лічильника TIM = 1 µm на дисплеї.
 *
 * У Flash зберігається не «сирий» TIM->CNT (після power-on він часто 0, а опора — стара → зрив),
 * а зміщення в мкм: відображувана позиція = pos_um + nv_offset_um.
 *
 * Запис у сектор 7 @ 0x08060000 (поза 384K регіоном лінкера для 512K).
 */

#define LINENC_FLASH_ADDR  0x08060000UL
#define LINENC_MAGIC       0x4C4E4332UL /* "LNC2" */

typedef struct {
    uint32_t magic;
    int32_t  offset_um_x;
    int32_t  offset_um_z;
    uint32_t _pad; /* 16 байт даних + magic = рівно 4 слова для простого програмування */
} linenc_nv_t;

/* Накопичена позиція від дельт TIM (як раніше) */
static volatile int32_t pos_um[2];
static uint32_t last_cnt2;
static uint16_t last_cnt3;

/* Зміщення з Flash (мкм), додається до pos_um */
static int32_t s_nv_offset_um[2];

/* Дзеркало в RTC backup: якщо Flash не пише (WRP) — нуль лишається після reset; з VBAT — часто й після power cycle. */
#define LINENC_BKP_MAGIC   0x4C4EU
#define BKP10R_OFS         0x78U
#define BKP11R_OFS         0x7CU
#define BKP12R_OFS         0x80U

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

static void linenc_bkp_save(void)
{
    backup_domain_enable();
    *(__IO uint32_t *)(RTC_BASE + BKP10R_OFS) = ((uint32_t)LINENC_BKP_MAGIC << 16) | 1u;
    *(__IO uint32_t *)(RTC_BASE + BKP11R_OFS) = (uint32_t)s_nv_offset_um[AXIS_X];
    *(__IO uint32_t *)(RTC_BASE + BKP12R_OFS) = (uint32_t)s_nv_offset_um[AXIS_Z];
}

static void linenc_bkp_load(void)
{
    backup_domain_enable();
    uint32_t w10 = *(__IO uint32_t *)(RTC_BASE + BKP10R_OFS);
    if ((w10 >> 16) != (uint32_t)LINENC_BKP_MAGIC)
        return;
    s_nv_offset_um[AXIS_X] = (int32_t)(*(__IO uint32_t *)(RTC_BASE + BKP11R_OFS));
    s_nv_offset_um[AXIS_Z] = (int32_t)(*(__IO uint32_t *)(RTC_BASE + BKP12R_OFS));
}

static void linenc_nv_load(void)
{
    s_nv_offset_um[AXIS_X] = 0;
    s_nv_offset_um[AXIS_Z] = 0;
    const linenc_nv_t *p = (const linenc_nv_t *)LINENC_FLASH_ADDR;
    if (p->magic == LINENC_MAGIC) {
        s_nv_offset_um[AXIS_X] = p->offset_um_x;
        s_nv_offset_um[AXIS_Z] = p->offset_um_z;
        return;
    }
    /* Flash порожній або старий формат (LNC1 / TIM ref) — пробуємо backup */
    linenc_bkp_load();
}

static int linenc_nv_save(void)
{
    linenc_nv_t w;
    w.magic = LINENC_MAGIC;
    w.offset_um_x = s_nv_offset_um[AXIS_X];
    w.offset_um_z = s_nv_offset_um[AXIS_Z];
    w._pad = 0xFFFFFFFFu;

    __disable_irq();

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef er = {0};
    uint32_t sect_err = 0;
    er.TypeErase = FLASH_TYPEERASE_SECTORS;
    er.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    /* 0x08060000 = старт останніх ~128К FLASH, для STM32F407 це відповідає SECTOR_11 */
    er.Sector = FLASH_SECTOR_11;
    er.NbSectors = 1u;

    if (HAL_FLASHEx_Erase(&er, &sect_err) != HAL_OK) {
        HAL_FLASH_Lock();
        __enable_irq();
        linenc_bkp_save();
        return -1;
    }

    uint32_t a = LINENC_FLASH_ADDR;
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, a, (uint64_t)w.magic) != HAL_OK) {
        HAL_FLASH_Lock();
        __enable_irq();
        linenc_bkp_save();
        return -1;
    }
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, a + 4u, (uint64_t)(uint32_t)w.offset_um_x) != HAL_OK) {
        HAL_FLASH_Lock();
        __enable_irq();
        linenc_bkp_save();
        return -1;
    }
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, a + 8u, (uint64_t)(uint32_t)w.offset_um_z) != HAL_OK) {
        HAL_FLASH_Lock();
        __enable_irq();
        linenc_bkp_save();
        return -1;
    }
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, a + 12u, (uint64_t)w._pad) != HAL_OK) {
        HAL_FLASH_Lock();
        __enable_irq();
        linenc_bkp_save();
        return -1;
    }

    HAL_FLASH_Lock();
    /* Після запису в ту ж банку Flash — скинути кеш (HAL уже у Erase; дубль безпечний) */
#if defined(__HAL_FLASH_INSTRUCTION_CACHE_DISABLE)
    __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
    __HAL_FLASH_INSTRUCTION_CACHE_RESET();
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
#endif
    __enable_irq();
    /* Дзеркало в BKP: працює після NVIC reset; при повному знятті живлення без VBAT — як Flash */
    linenc_bkp_save();
    return 0;
}

void axis_feedback_init(void)
{
    linenc_nv_load();
    pos_um[AXIS_X] = 0;
    pos_um[AXIS_Z] = 0;
    last_cnt2 = (uint32_t)TIM2->CNT;
    last_cnt3 = (uint16_t)TIM3->CNT;
}

void axis_feedback_tick_1ms(void)
{
    uint32_t c2 = (uint32_t)TIM2->CNT;
    int32_t d2 = (int32_t)(c2 - last_cnt2);
    last_cnt2 = c2;
    pos_um[AXIS_X] += d2;

    uint16_t c3 = (uint16_t)TIM3->CNT;
    int16_t d3 = (int16_t)(c3 - last_cnt3);
    last_cnt3 = c3;
    pos_um[AXIS_Z] += (int32_t)d3;
}

float axis_feedback_pos_mm(axis_id_t axis)
{
    return (float)axis_feedback_pos_um(axis) * 0.001f;
}

int32_t axis_feedback_pos_um(axis_id_t axis)
{
    if (axis == AXIS_X)
        return pos_um[AXIS_X] + s_nv_offset_um[AXIS_X];
    if (axis == AXIS_Z)
        return pos_um[AXIS_Z] + s_nv_offset_um[AXIS_Z];
    return 0;
}

void axis_feedback_zero(axis_id_t axis)
{
    /*
     * Щоб показ 0 зберігався після power-on: після старту pos_um знову 0.
     * Якщо лише змінювати nv_offset за формулою off -= (pos+off), у Flash лишається off = -pos,
     * і після перезавантаження display = 0 + (-pos_old) ≠ 0.
     *
     * Тому: скинути інтегратор осі + nv_offset цієї осі в 0 (як «G54 у цій точці»).
     */
    __disable_irq();
    if (axis == AXIS_X) {
        s_nv_offset_um[AXIS_X] = 0;
        pos_um[AXIS_X] = 0;
        last_cnt2 = (uint32_t)TIM2->CNT;
    } else if (axis == AXIS_Z) {
        s_nv_offset_um[AXIS_Z] = 0;
        pos_um[AXIS_Z] = 0;
        last_cnt3 = (uint16_t)TIM3->CNT;
    }
    __enable_irq();
    (void)linenc_nv_save();
}
