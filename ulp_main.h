#include "Arduino.h"

extern volatile uint32_t ulp_entry;

// variables used to exchange data between main CPU and ULP
extern volatile uint32_t ulp_active_card_mutex_by_main_cpu;
extern volatile uint32_t ulp_active_card_mutex_by_ulp_cpu;
extern volatile uint32_t ulp_active_card_id_lo;
extern volatile uint32_t ulp_active_card_id_hi;

extern volatile uint32_t ulp_main_cpu_sleeps;
extern volatile uint32_t ulp_vbatt;
extern volatile uint32_t ulp_vbatt_low;

// variables only used by main CPU to persist data during deep sleep
extern volatile uint32_t ulp_current_volume;
extern volatile uint32_t ulp_last_card_id;
extern volatile uint32_t ulp_last_track;
extern volatile uint32_t ulp_last_file_position;
