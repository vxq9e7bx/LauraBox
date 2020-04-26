#include "Arduino.h"

extern volatile uint32_t ulp_entry;
extern volatile uint32_t ulp_active_card_mutex_by_main_cpu;
extern volatile uint32_t ulp_active_card_mutex_by_ulp_cpu;
extern volatile uint32_t ulp_active_card_id_lo;
extern volatile uint32_t ulp_active_card_id_hi;
extern volatile uint32_t ulp_main_cpu_sleeps;
