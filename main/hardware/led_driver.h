#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "app_driver.h"
#include "led_strip.h"
#include "esp_err.h"

// Function prototypes
void led_driver_init(void);
void led_driver_on(void);
void led_driver_off(void);
void led_obstacle_warning(void);
void led_obstacle_alert(void);
void led_vehicle_parked(void);
void led_vehicle_leaving(void);
void led_barrier_locking(void);
void led_barrier_unlocking(void);
void led_connection_lost(void);
void led_connection_established(void);
void led_stop(void);
void led_spot_empty(void);

#endif // LED_DRIVER_H
