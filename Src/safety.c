#include "safety.h"
#include "buttons.h"
#include "system_state.h"
#include "fault.h"

void safety_init(void){}

void safety_process(void)
{
    if (buttons_estop()) {
        fault_set(1);
        system_request_error(1);
    }
}
