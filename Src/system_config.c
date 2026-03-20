#include "system_config.h"

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
}

const axis_cfg_t* system_axis_cfg(axis_id_t a)
{
    return &cfg[a];
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
