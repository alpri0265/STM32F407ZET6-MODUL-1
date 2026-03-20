#include "jog.h"
#include "main.h"
#include "board.h"
#include "manual_feed_mode.h"
#include "enc_if.h"
#include "adc_if.h"
#include "bringup_config.h"
#include "menu.h"
#include "system_config.h"
#include "sl_limits.h"
#include <stdint.h>
#include <stdbool.h>

#if BRINGUP_MODE

/* Підключення до драйвера (DM556): PUL = STEP, DIR = напрямок, ENA = дозвіл.
   В main.h: X — PA8=PUL, PA9=DIR, PA10=ENA; Z — PB6=PUL, PB7=DIR, PB8=ENA. */
#define JOG_STEP_PERIOD_MS       4u  /* мс між кроками (база) — більший діапазон для rapid */
#define JOG_STEP_PERIOD_RAPID_MS 1u  /* мс при rapid — завжди 2–4× швидше */
/* Feed override: ADC 0..4095 → scale 50..150%. period = base*100/scale, min 1 ms */
#define JOG_PULSE_CYCLES    500u   /* тривалість імпульсу ~3 µs при 168 MHz (DM556: min 2.5 µs) */
#define JOG_DIR_SETTLE      400u   /* циклів після DIR перед STEP (~2.4 µs) */
#define SL_OSC_HYST_MM      1.0f   /* гістерезис при розвороті (мм) — уникнути вібрації */
#define SL_OSC_MIN_RANGE_MM 2.0f   /* мін. відстань між лімітами для авто-коливань */

/* 1 = імпульс кроку активний по LOW (idle HIGH, pulse LOW); 0 = активний по HIGH */
#define STEP_PULSE_ACTIVE_LOW  0
/* 1 = ENA по HIGH увімкнено; 0 = ENA по LOW увімкнено */
#define ENA_ACTIVE_HIGH        1
/* 1 = постійні кроки X/Z без джойстика (тест); 0 = рух тільки від джойстика */
#define JOG_ALWAYS_RUN_TEST    0
/* Кнопка пришвидшеної подачі: використовуємо SCALE_0 (PC0). Натиснуто = LOW (на землю). */
#define RAPID_BTN_GPIO_Port    SCALE_0_GPIO_Port
#define RAPID_BTN_Pin          SCALE_0_Pin
#define RAPID_BTN_ACTIVE_LOW   1

static volatile uint32_t s_isr_tick;   /* +1 кожні 1 ms від TIM6 — рівномірний таймінг */
static uint32_t s_last_step_tick;
static volatile uint8_t s_manual_mode; /* 1 = ручна подача (ISR крокує), 0 = Feed Auto (ISR крокує ліміт→ліміт) */
static volatile uint8_t s_limit_block; /* 0x1=-X 0x2=+X 0x4=-Z 0x8=+Z — блокування руху */
static uint32_t s_step_count_x;
static uint32_t s_step_count_z;
/* Підписані позиції в кроках для програмних лімітів */
static int32_t s_pos_x_steps;
static int32_t s_pos_z_steps;
static uint16_t s_feed_override_cached = 2048u;  /* 50% по замовчуванню, оновлюється з main loop */
/* Кеш steps_per_mm для ISR (позиція в мм у Feed Auto) */
static float s_steps_per_mm_x = 1.0f;
static float s_steps_per_mm_z = 1.0f;
static int s_feed_auto_x_dir = 1;
static int s_feed_auto_z_dir = 1;
/* Z: скільки проходів між лімітами (0 = без обмежень). */
static volatile uint8_t s_z_passes_requested = 0u;
static volatile uint16_t s_z_reversal_count = 0u;
/* X: кількість проходів (0 = без обмежень). Крок заглиблення 0.01..0.3 mm (індекс 0..29) — після кожного повного проходу по Z вісь X зміщується на цей крок у напрямку X- (принцип токарного станка). */
static volatile uint8_t s_x_passes_requested = 0u;
static volatile uint16_t s_x_reversal_count = 0u;
static volatile uint8_t s_x_step_per_pass_index = 0u;  /* 0..29 → 0.01, 0.02, ... 0.30 mm */
static volatile int32_t s_z_pass_x_pending_steps = 0;  /* X кроків у напрямку X- після проходу Z (дренуємо по одному за тик) */

static void step_pulse_delay(void)
{
    for (volatile uint32_t i = 0u; i < JOG_PULSE_CYCLES; i++) (void)0;
}

static void dir_settle_delay(void)
{
    for (volatile uint32_t i = 0u; i < JOG_DIR_SETTLE; i++) (void)0;
}

static bool rapid_pressed(void)
{
    GPIO_PinState s = HAL_GPIO_ReadPin(RAPID_BTN_GPIO_Port, RAPID_BTN_Pin);
    if (RAPID_BTN_ACTIVE_LOW)
        return (s == GPIO_PIN_RESET);
    else
        return (s == GPIO_PIN_SET);
}

static void pulse_x_step(int dir)
{
#if STEP_PULSE_ACTIVE_LOW
    HAL_GPIO_WritePin(X_STEP_GPIO_Port, X_STEP_Pin, GPIO_PIN_RESET);
    step_pulse_delay();
    HAL_GPIO_WritePin(X_STEP_GPIO_Port, X_STEP_Pin, GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(X_STEP_GPIO_Port, X_STEP_Pin, GPIO_PIN_SET);
    step_pulse_delay();
    HAL_GPIO_WritePin(X_STEP_GPIO_Port, X_STEP_Pin, GPIO_PIN_RESET);
#endif
    s_step_count_x++;
    s_pos_x_steps += dir;
}

static void pulse_z_step(int dir)
{
#if STEP_PULSE_ACTIVE_LOW
    HAL_GPIO_WritePin(z_STEP_GPIO_Port, z_STEP_Pin, GPIO_PIN_RESET);
    step_pulse_delay();
    HAL_GPIO_WritePin(z_STEP_GPIO_Port, z_STEP_Pin, GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(z_STEP_GPIO_Port, z_STEP_Pin, GPIO_PIN_SET);
    step_pulse_delay();
    HAL_GPIO_WritePin(z_STEP_GPIO_Port, z_STEP_Pin, GPIO_PIN_RESET);
#endif
    s_step_count_z++;
    s_pos_z_steps += dir;
}

void jog_init(void)
{
    s_isr_tick = 0u;
    s_last_step_tick = 0u;
    s_manual_mode = 1u;
    s_pos_x_steps = 0;
    s_pos_z_steps = 0;
    {
        const axis_cfg_t *cx = system_axis_cfg(AXIS_X);
        const axis_cfg_t *cz = system_axis_cfg(AXIS_Z);
        s_steps_per_mm_x = (cx->steps_per_mm > 0.0f) ? cx->steps_per_mm : 1.0f;
        s_steps_per_mm_z = (cz->steps_per_mm > 0.0f) ? cz->steps_per_mm : 1.0f;
    }
    /* STEP (PUL) — переводимо з AF TIM1/TIM4 у звичайний вихід GPIO */
    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Pin = X_STEP_Pin;
    HAL_GPIO_Init(X_STEP_GPIO_Port, &g);
    g.Pin = z_STEP_Pin;
    HAL_GPIO_Init(z_STEP_GPIO_Port, &g);
#if STEP_PULSE_ACTIVE_LOW
    HAL_GPIO_WritePin(X_STEP_GPIO_Port, X_STEP_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(z_STEP_GPIO_Port, z_STEP_Pin, GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(X_STEP_GPIO_Port, X_STEP_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(z_STEP_GPIO_Port, z_STEP_Pin, GPIO_PIN_RESET);
#endif
#if ENA_ACTIVE_HIGH
    HAL_GPIO_WritePin(X_EN_GPIO_Port, X_EN_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(Z_EN_GPIO_Port, Z_EN_Pin, GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(X_EN_GPIO_Port, X_EN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(Z_EN_GPIO_Port, Z_EN_Pin, GPIO_PIN_RESET);
#endif

    /* Кнопка Rapid (SCALE_0 / PC0): вхід з підтяжкою вгору, натиснуто = до GND. */
    GPIO_InitTypeDef b = {0};
    b.Pin = RAPID_BTN_Pin;
    b.Mode = GPIO_MODE_INPUT;
    b.Pull = GPIO_PULLUP;
    b.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RAPID_BTN_GPIO_Port, &b);
}

void jog_update_steps_per_mm_from_cfg(void)
{
    /* ISR використовує кеш s_steps_per_mm_x/z, тому оновлюємо його при редагуванні параметрів. */
    const axis_cfg_t *cx = system_axis_cfg(AXIS_X);
    const axis_cfg_t *cz = system_axis_cfg(AXIS_Z);
    s_steps_per_mm_x = (cx->steps_per_mm > 0.0f) ? cx->steps_per_mm : 1.0f;
    s_steps_per_mm_z = (cz->steps_per_mm > 0.0f) ? cz->steps_per_mm : 1.0f;
}

/* 1 = рух від джойстика (JOY_UP/DOWN/LEFT/RIGHT), 0 = рух від кнопок меню */
#define JOG_USE_JOYSTICK 1

#if JOG_USE_JOYSTICK
/* 1 = натиснуто коли пін LOW, 0 = натиснуто коли пін HIGH. За показами U/D/L/R на екрані Jog */
#define JOY_UP_ACTIVE_LOW    1
#define JOY_DOWN_ACTIVE_LOW  1
#define JOY_LEFT_ACTIVE_LOW  1
#define JOY_RIGHT_ACTIVE_LOW 1
static bool joy_up(void)    { return (HAL_GPIO_ReadPin(JOY_UP_GPIO_Port, JOY_UP_Pin) == GPIO_PIN_RESET) == (JOY_UP_ACTIVE_LOW != 0); }
static bool joy_down(void)  { return (HAL_GPIO_ReadPin(JOY_DOWN_GPIO_Port, JOY_DOWN_Pin) == GPIO_PIN_RESET) == (JOY_DOWN_ACTIVE_LOW != 0); }
static bool joy_left(void)  { return (HAL_GPIO_ReadPin(JOY_LEFT_GPIO_Port, JOY_LEFT_Pin) == GPIO_PIN_RESET) == (JOY_LEFT_ACTIVE_LOW != 0); }
static bool joy_right(void) { return (HAL_GPIO_ReadPin(JOY_RIGHT_GPIO_Port, JOY_RIGHT_Pin) == GPIO_PIN_RESET) == (JOY_RIGHT_ACTIVE_LOW != 0); }

void jog_get_joy_state(unsigned int *up, unsigned int *down, unsigned int *left, unsigned int *right)
{
    if (up)   *up   = joy_up()   ? 1u : 0u;
    if (down) *down = joy_down() ? 1u : 0u;
    if (left) *left = joy_left() ? 1u : 0u;
    if (right) *right = joy_right() ? 1u : 0u;
}

void jog_get_step_counts(uint32_t *x, uint32_t *z)
{
    if (x) *x = s_step_count_x;
    if (z) *z = s_step_count_z;
}

void jog_get_pos_mm(float *x_mm, float *z_mm)
{
    const axis_cfg_t *cx = system_axis_cfg(AXIS_X);
    const axis_cfg_t *cz = system_axis_cfg(AXIS_Z);
    if (x_mm) *x_mm = (float)s_pos_x_steps / (cx->steps_per_mm > 0.0f ? cx->steps_per_mm : 1.0f);
    if (z_mm) *z_mm = (float)s_pos_z_steps / (cz->steps_per_mm > 0.0f ? cz->steps_per_mm : 1.0f);
}

void jog_get_rapid_state(unsigned int *rapid)
{
    if (rapid) *rapid = rapid_pressed() ? 1u : 0u;
}

void jog_set_feed_override(uint16_t raw)
{
    s_feed_override_cached = raw;
}

unsigned int jog_get_z_passes(void)
{
    return (unsigned int)s_z_passes_requested;
}

void jog_set_z_passes(unsigned int n)
{
    s_z_passes_requested = (n > 99u) ? 99u : (uint8_t)n;
}

unsigned int jog_get_x_passes(void)
{
    return (unsigned int)s_x_passes_requested;
}

void jog_set_x_passes(unsigned int n)
{
    s_x_passes_requested = (n > 99u) ? 99u : (uint8_t)n;
}

/* Індекс кроку 0..29 → 0.01, 0.02, ... 0.30 mm */
unsigned int jog_get_x_step_index(void)
{
    return (unsigned int)s_x_step_per_pass_index;
}

void jog_set_x_step_index(unsigned int i)
{
    s_x_step_per_pass_index = (i > 29u) ? 29u : (uint8_t)i;
}

/* Крок заглиблення X в мм за індексом 0..29 (0.01 .. 0.30) */
static float x_step_mm_from_index(unsigned int i)
{
    if (i > 29u) i = 29u;
    return 0.01f * (float)(i + 1u);
}

/* Ручний режим: рух від енкодера RE60 (MPG_A/B).
 * Використовуємо фронт A (PB12) і стан B (PB13) для визначення напрямку:
 * A: 1->0, B==1 → крок +
 * A: 1->0, B==0 → крок -.
 */
static void handle_manual_encoder_mode(void)
{
    encoder_axis_t ax = manual_feed_get_axis();
    if (ax == ENCODER_AXIS_NONE)
        return;

    static uint8_t last_a = 1u;
    uint8_t a_now = (HAL_GPIO_ReadPin(MPG_A_GPIO_Port, MPG_A_Pin) == GPIO_PIN_RESET) ? 0u : 1u;
    uint8_t b_now = (HAL_GPIO_ReadPin(MPG_B_GPIO_Port, MPG_B_Pin) == GPIO_PIN_RESET) ? 0u : 1u;

    int dir_enc = 0;
    if (last_a == 1u && a_now == 0u) {
        /* Фронт A: напрямок залежить від поточного стану B */
        dir_enc = (b_now ? +1 : -1);
    }
    last_a = a_now;

    if (dir_enc == 0)
        return;

    step_scale_t sc = manual_feed_get_step_scale();
    float scale_mm;
    switch (sc) {
        case STEP_SCALE_0_001: scale_mm = 0.001f; break;
        case STEP_SCALE_0_01:  scale_mm = 0.01f;  break;
        default:               scale_mm = 0.1f;   break;
    }

    float steps_per_mm = (ax == ENCODER_AXIS_X) ? s_steps_per_mm_x : s_steps_per_mm_z;
    float steps_f = (float)dir_enc * scale_mm * steps_per_mm;
    int32_t steps = (steps_f >= 0.0f) ? (int32_t)(steps_f + 0.5f) : (int32_t)(steps_f - 0.5f);
    if (steps == 0)
        return;

    int dir = (steps >= 0) ? 1 : -1;
    uint32_t n = (steps >= 0) ? (uint32_t)steps : (uint32_t)(-steps);

    while (n--) {
        if (ax == ENCODER_AXIS_X) {
            HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, dir > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
            dir_settle_delay();
            pulse_x_step(dir);
        } else {
            HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, dir > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
            dir_settle_delay();
            pulse_z_step(dir);
        }
    }
}

/* Генерація кроків з TIM6. При s_manual_mode==0 (Feed Auto) тут же робимо кроки ліміт→ліміт. */
void jog_tick_from_isr(void)
{
    /* У ручному режимі (MANUAL) рух від енкодера, джойстик/Feed Auto не крокують. */
    if (manual_feed_get_mode() == FEED_MODE_MANUAL)
        return;

    s_isr_tick++;

    uint32_t base_ms = rapid_pressed() ? JOG_STEP_PERIOD_RAPID_MS : JOG_STEP_PERIOD_MS;
    uint16_t feed_raw = s_feed_override_cached;
    uint32_t scale = 30u + ((uint32_t)feed_raw * 120u) / 4095u;
    if (scale < 10u) scale = 10u;
    uint32_t period_ms = (base_ms * 100u) / scale;
    if (period_ms < 1u) period_ms = 1u;
    if ((s_isr_tick - s_last_step_tick) < period_ms)
        return;

    if (s_manual_mode == 0u) {
        /* Дренування X-кроків (заглиблення X-) після проходу Z — один крок за тик */
        if (s_z_pass_x_pending_steps > 0) {
            s_last_step_tick = s_isr_tick;
            HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_RESET);  /* X- */
            dir_settle_delay();
            pulse_x_step(-1);
            s_z_pass_x_pending_steps--;
            return;
        }

        float xm = (float)s_pos_x_steps / s_steps_per_mm_x;
        float zm = (float)s_pos_z_steps / s_steps_per_mm_z;

        if (joy_up()) {
            if (sl_limits_z_taught()) {
                float lo = sl_limits_get_z_min();
                float hi = sl_limits_get_z_max();
                if (lo > hi) { float t = lo; lo = hi; hi = t; }
                if ((hi - lo) < SL_OSC_MIN_RANGE_MM) {
                    s_last_step_tick = s_isr_tick;
                    return;
                }
                if (s_z_passes_requested != 0u && s_z_reversal_count >= 2u * (uint16_t)s_z_passes_requested) {
                    s_last_step_tick = s_isr_tick;
                    return;  /* задана кількість проходів Z виконана */
                }
                {
                    int old_z = s_feed_auto_z_dir;
                    if (zm >= hi - SL_OSC_HYST_MM) s_feed_auto_z_dir = -1;
                    else if (zm <= lo + SL_OSC_HYST_MM) s_feed_auto_z_dir = 1;
                    if (s_feed_auto_z_dir != old_z) {
                        s_z_reversal_count++;
                        /* Після кожного повного проходу Z (туди-назад) — заглиблення по X на крок (X-) */
                        if (s_z_reversal_count >= 2u && (s_z_reversal_count & 1u) == 0u) {
                            float step_mm = x_step_mm_from_index(s_x_step_per_pass_index);
                            int32_t x_steps = (int32_t)(step_mm * s_steps_per_mm_x + 0.5f);
                            if (x_steps > 0) s_z_pass_x_pending_steps += x_steps;
                        }
                    }
                    s_last_step_tick = s_isr_tick;
                    HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, s_feed_auto_z_dir > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
                    dir_settle_delay();
                    pulse_z_step(s_feed_auto_z_dir);
                }
            } else {
                s_last_step_tick = s_isr_tick;
                HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_SET);
                dir_settle_delay();
                pulse_z_step(1);
            }
            return;
        }
        if (joy_down()) {
            if (sl_limits_z_taught()) {
                float lo = sl_limits_get_z_min();
                float hi = sl_limits_get_z_max();
                if (lo > hi) { float t = lo; lo = hi; hi = t; }
                if ((hi - lo) < SL_OSC_MIN_RANGE_MM) {
                    s_last_step_tick = s_isr_tick;
                    return;
                }
                if (s_z_passes_requested != 0u && s_z_reversal_count >= 2u * (uint16_t)s_z_passes_requested) {
                    s_last_step_tick = s_isr_tick;
                    return;
                }
                {
                    int old_z = s_feed_auto_z_dir;
                    if (zm >= hi - SL_OSC_HYST_MM) s_feed_auto_z_dir = -1;
                    else if (zm <= lo + SL_OSC_HYST_MM) s_feed_auto_z_dir = 1;
                    if (s_feed_auto_z_dir != old_z) {
                        s_z_reversal_count++;
                        if (s_z_reversal_count >= 2u && (s_z_reversal_count & 1u) == 0u) {
                            float step_mm = x_step_mm_from_index(s_x_step_per_pass_index);
                            int32_t x_steps = (int32_t)(step_mm * s_steps_per_mm_x + 0.5f);
                            if (x_steps > 0) s_z_pass_x_pending_steps += x_steps;
                        }
                    }
                    s_last_step_tick = s_isr_tick;
                    HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, s_feed_auto_z_dir > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
                    dir_settle_delay();
                    pulse_z_step(s_feed_auto_z_dir);
                }
            } else {
                s_last_step_tick = s_isr_tick;
                HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_RESET);
                dir_settle_delay();
                pulse_z_step(-1);
            }
            return;
        }
        if (joy_left()) {
            if (sl_limits_x_taught()) {
                float lo = sl_limits_get_x_min();
                float hi = sl_limits_get_x_max();
                if (lo > hi) { float t = lo; lo = hi; hi = t; }
                if ((hi - lo) < SL_OSC_MIN_RANGE_MM) {
                    s_last_step_tick = s_isr_tick;
                    return;
                }
                if (s_x_passes_requested != 0u && s_x_reversal_count >= 2u * (uint16_t)s_x_passes_requested) {
                    s_last_step_tick = s_isr_tick;
                    return;
                }
                {
                    int old_x = s_feed_auto_x_dir;
                    if (xm >= hi - SL_OSC_HYST_MM) s_feed_auto_x_dir = -1;
                    else if (xm <= lo + SL_OSC_HYST_MM) s_feed_auto_x_dir = 1;
                    if (s_feed_auto_x_dir != old_x) s_x_reversal_count++;
                    s_last_step_tick = s_isr_tick;
                    HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, s_feed_auto_x_dir > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
                    dir_settle_delay();
                    pulse_x_step(s_feed_auto_x_dir);
                }
            } else {
                s_last_step_tick = s_isr_tick;
                HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_RESET);
                dir_settle_delay();
                pulse_x_step(-1);
            }
            return;
        }
        if (joy_right()) {
            if (sl_limits_x_taught()) {
                float lo = sl_limits_get_x_min();
                float hi = sl_limits_get_x_max();
                if (lo > hi) { float t = lo; lo = hi; hi = t; }
                if ((hi - lo) < SL_OSC_MIN_RANGE_MM) {
                    s_last_step_tick = s_isr_tick;
                    return;
                }
                if (s_x_passes_requested != 0u && s_x_reversal_count >= 2u * (uint16_t)s_x_passes_requested) {
                    s_last_step_tick = s_isr_tick;
                    return;
                }
                {
                    int old_x = s_feed_auto_x_dir;
                    if (xm >= hi - SL_OSC_HYST_MM) s_feed_auto_x_dir = -1;
                    else if (xm <= lo + SL_OSC_HYST_MM) s_feed_auto_x_dir = 1;
                    if (s_feed_auto_x_dir != old_x) s_x_reversal_count++;
                    s_last_step_tick = s_isr_tick;
                    HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, s_feed_auto_x_dir > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
                    dir_settle_delay();
                    pulse_x_step(s_feed_auto_x_dir);
                }
            } else {
                s_last_step_tick = s_isr_tick;
                HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_SET);
                dir_settle_delay();
                pulse_x_step(1);
            }
            return;
        }
        /* Нейтраль: без руху, тільки скидання лічильників реверсів */
        s_z_reversal_count = 0u;
        s_x_reversal_count = 0u;
        s_last_step_tick = s_isr_tick;
        return;
    }

    s_last_step_tick = s_isr_tick;
    if (joy_up()) {
        HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_SET);
        dir_settle_delay();
        pulse_z_step(1);
        return;
    }
    if (joy_down()) {
        HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_RESET);
        dir_settle_delay();
        pulse_z_step(-1);
        return;
    }
    if (joy_left()) {
        HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_RESET);
        dir_settle_delay();
        pulse_x_step(-1);
        return;
    }
    if (joy_right()) {
        HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_SET);
        dir_settle_delay();
        pulse_x_step(1);
    }
}
#else
void jog_get_joy_state(unsigned int *up, unsigned int *down, unsigned int *left, unsigned int *right)
{
    if (up)   *up   = 0u;
    if (down) *down = 0u;
    if (left) *left = 0u;
    if (right) *right = 0u;
}
void jog_get_step_counts(uint32_t *x, uint32_t *z)
{
    if (x) *x = s_step_count_x;
    if (z) *z = s_step_count_z;
}
void jog_get_pos_mm(float *x_mm, float *z_mm)
{
    const axis_cfg_t *cx = system_axis_cfg(AXIS_X);
    const axis_cfg_t *cz = system_axis_cfg(AXIS_Z);
    if (x_mm) *x_mm = (float)s_pos_x_steps / (cx->steps_per_mm > 0.0f ? cx->steps_per_mm : 1.0f);
    if (z_mm) *z_mm = (float)s_pos_z_steps / (cz->steps_per_mm > 0.0f ? cz->steps_per_mm : 1.0f);
}
void jog_get_rapid_state(unsigned int *rapid)
{
    if (rapid) *rapid = 0u;
}
void jog_set_feed_override(uint16_t raw)
{
    s_feed_override_cached = raw;
}
void jog_tick_from_isr(void) { (void)0; }  /* заглушка: джойстик не використовується */
#endif

void jog_process(void)
{
    feed_mode_t mode = manual_feed_get_mode();

    if (mode == FEED_MODE_MANUAL) {
        handle_manual_encoder_mode();
        return;
    }

    s_manual_mode = 1u;   /* за замовч. джойстик працює; 0 лише для Feed Auto */
    menu_screen_id_t scr = menu_current_screen();

#if JOG_ALWAYS_RUN_TEST
    {
        uint32_t now = s_isr_tick;
        uint32_t period_ms = JOG_STEP_PERIOD_MS;
        if ((now - s_last_step_tick) < period_ms)
            return;
        s_last_step_tick = now;
        static int alt;
        if (alt) {
            HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_SET);
            dir_settle_delay();
            pulse_z_step(1);
        } else {
            HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_SET);
            dir_settle_delay();
            pulse_x_step(1);
        }
        alt = 1 - alt;
        return;
    }
#endif

#if JOG_USE_JOYSTICK
    if (scr == SCREEN_FEED_AUTO) {
        s_manual_mode = 0u;  /* кроки ліміт→ліміт робить TIM6 ISR */
        return;
    }

    /* Jog/Feed/Feed Manual: ISR крокує. Тут лише оновлюємо блокування лімітів. */
    s_manual_mode = 1u;
    {
        float xm, zm;
        jog_get_pos_mm(&xm, &zm);
        uint8_t block = 0u;
        if (sl_limits_x_taught()) {
            float hi = sl_limits_get_x_max();
            float lo = sl_limits_get_x_min();
            if (lo > hi) { float t = lo; lo = hi; hi = t; }
            if (xm <= lo + 0.2f) block |= 0x1u;  /* -X */
            if (xm >= hi - 0.2f) block |= 0x2u;  /* +X */
        }
        if (sl_limits_z_taught()) {
            float hi = sl_limits_get_z_max();
            float lo = sl_limits_get_z_min();
            if (lo > hi) { float t = lo; lo = hi; hi = t; }
            if (zm <= lo + 0.2f) block |= 0x4u;  /* -Z */
            if (zm >= hi - 0.2f) block |= 0x8u;  /* +Z */
        }
        s_limit_block = block;
    }
#else
    /* Режим кнопок меню: Enter перемикає вісь (X/Z), Up = +, Down = −. PB12=Up, PB13=Down, PB14=Enter */
    static int axis_mode; /* 0=X, 1=Z */
    static bool last_ent;
    bool up   = (HAL_GPIO_ReadPin(MPG_A_GPIO_Port, MPG_A_Pin) == GPIO_PIN_RESET);
    bool down = (HAL_GPIO_ReadPin(MPG_B_GPIO_Port, MPG_B_Pin) == GPIO_PIN_RESET);
    bool ent  = (HAL_GPIO_ReadPin(MPG_BTN_GPIO_Port, MPG_BTN_Pin) == GPIO_PIN_SET);

    if (ent && !last_ent) {
        axis_mode = 1 - axis_mode;
        s_last_step_tick = now;
    }
    last_ent = ent;
    if (ent) return;
    if (axis_mode == 0) {
        if (up) {
            HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_SET);
            dir_settle_delay();
            pulse_x_step(1);
        } else if (down) {
            HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_RESET);
            dir_settle_delay();
            pulse_x_step(-1);
        }
    } else {
        if (up) {
            HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_SET);
            dir_settle_delay();
            pulse_z_step(1);
        } else if (down) {
            HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_RESET);
            dir_settle_delay();
            pulse_z_step(-1);
        }
    }
#endif
}

#else
void jog_init(void) {}
void jog_process(void) {}
void jog_get_joy_state(unsigned int *up, unsigned int *down, unsigned int *left, unsigned int *right)
{
    if (up)   *up   = 0u;
    if (down) *down = 0u;
    if (left) *left = 0u;
    if (right) *right = 0u;
}
void jog_get_step_counts(uint32_t *x, uint32_t *z)
{
    if (x) *x = 0u;
    if (z) *z = 0u;
}
void jog_get_pos_mm(float *x_mm, float *z_mm)
{
    if (x_mm) *x_mm = 0.0f;
    if (z_mm) *z_mm = 0.0f;
}
void jog_get_rapid_state(unsigned int *rapid)
{
    if (rapid) *rapid = 0u;
}
void jog_set_feed_override(uint16_t raw) { (void)raw; }
#endif
