#ifndef APP_DRIVER_H
#define APP_DRIVER_H
#include <stdint.h>
#include <stdbool.h> 
void app_driver_init(void);
void app_driver_led_on(void);
void app_driver_led_off(void);
void app_driver_barrier_open(void);
void app_driver_barrier_close(void);
void app_driver_led_disconnected(void);
void app_driver_led_connected(void);
void app_driver_led_locked(void);
void app_driver_led_unlocked(void);
void app_driver_barrier_changing(void);



int32_t app_driver_obstacle_distance(void);
bool app_driver_is_barrier_locked(void);
bool app_driver_is_barrier_unlocked(void);
/**
 * @brief Test the limit switches and log their status.
 */
void app_driver_test_limit_switches(void);
#endif // APP_DRIVER_H