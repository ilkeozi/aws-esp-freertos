#ifndef BUZZER_DRIVER_H
#define BUZZER_DRIVER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

// Function prototypes
void buzzer_driver_init(void);
void buzzer_play_note(int frequency, int duration);
void buzzer_obstacle_warning(void);
void buzzer_obstacle_alert(void);
void buzzer_vehicle_parked(void);
void buzzer_vehicle_leaving(void);
void buzzer_barrier_locking(void);
void buzzer_barrier_unlocking(void);
void buzzer_connection_lost(void);
void buzzer_connection_established(void);
void buzzer_stop(void);

#endif // BUZZER_DRIVER_H
