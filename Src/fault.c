#include "fault.h"
static uint16_t fault;
void fault_set(uint16_t c){ fault = c; }
uint16_t fault_get(void){ return fault; }
