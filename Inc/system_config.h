#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <stdint.h>

typedef enum {
    AXIS_X = 0,
    AXIS_Z = 1
} axis_id_t;

typedef struct {
    float steps_per_mm;
    float max_feed;
    float min_mm;
    float max_mm;
} axis_cfg_t;

void system_config_init(void);
void system_config_save(void);  /* Зберегти в RTC backup (Save & exit). */
const axis_cfg_t* system_axis_cfg(axis_id_t a);
void system_axis_set_max_feed(axis_id_t axis, float v);
void system_axis_set_min_mm(axis_id_t axis, float v);
void system_axis_set_max_mm(axis_id_t axis, float v);

/* Mechanics parameters (used to compute steps_per_mm). */
uint16_t system_mech_get_pitch_x100(axis_id_t axis);
void system_mech_set_pitch_x100(axis_id_t axis, uint16_t pitch_x100);
uint32_t system_mech_get_motor_steps_per_rev(axis_id_t axis);
void system_mech_set_motor_steps_per_rev(axis_id_t axis, uint32_t steps_per_rev);
uint16_t system_mech_get_microstep(axis_id_t axis);
void system_mech_set_microstep(axis_id_t axis, uint16_t microstep);
uint32_t system_mech_get_reducer_ratio_x1000(axis_id_t axis);
void system_mech_set_reducer_ratio_x1000(axis_id_t axis, uint32_t ratio_x1000);

#endif
