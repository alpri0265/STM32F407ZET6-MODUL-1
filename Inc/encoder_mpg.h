#ifndef ENCODER_MPG_H
#define ENCODER_MPG_H
#include <stdint.h>

void encoder_mpg_init(void);
void encoder_mpg_process(void);
int32_t encoder_mpg_delta(void);

#endif
