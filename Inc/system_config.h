#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

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
const axis_cfg_t* system_axis_cfg(axis_id_t a);

#endif
