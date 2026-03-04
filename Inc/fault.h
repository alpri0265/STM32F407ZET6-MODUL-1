#ifndef FAULT_H
#define FAULT_H
#include <stdint.h>
void fault_set(uint16_t code);
uint16_t fault_get(void);
#endif
