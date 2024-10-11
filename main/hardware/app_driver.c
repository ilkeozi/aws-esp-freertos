#include "app_driver.h"
#include "barrier_driver.h"
#include "led_driver.h"
#include "esp_log.h"
#include "hcsr04_sensor.h"

static const char *TAG = "app_driver";
static hcsr04_sensor_t hcsr04_sensor = {
    .trigger_pin = GPIO_NUM_5,  
    .echo_pin = GPIO_NUM_18     
};

void app_driver_init(void)
{
    ESP_LOGI(TAG, "Initializing the application driver.");

    barrier_driver_init();
    led_driver_init();
    hcsr04_sensor_init(&hcsr04_sensor);
   
}

void app_driver_barrier_open(void)
{
    barrier_driver_open();

}

void app_driver_barrier_close(void)
{
    barrier_driver_close();
    
}

void app_driver_led_on(void)
{
    led_driver_on();
}

void app_driver_led_off(void)
{
    led_driver_off();
}

int32_t app_driver_obstacle_distance(void) {
    uint32_t distance_cm;
    esp_err_t err = hcsr04_sensor_measure_cm(&hcsr04_sensor, 500, &distance_cm);

    if (err == ESP_OK) {
        return distance_cm;
    } else {
        return -1;
    }
}

bool app_driver_is_barrier_locked(void) {
    return barrier_driver_is_locked();
}

bool app_driver_is_barrier_unlocked(void) {
    return barrier_driver_is_unlocked();
}

void app_driver_test_limit_switches(void)
{
    barrier_driver_test_limit_switches();
}

void app_driver_led_disconnected(void){
    led_connection_lost();
}
void app_driver_led_connected(void)
{
    led_connection_established();
}
void app_driver_led_locked(void){
    led_spot_empty();
}
void app_driver_led_unlocked(void){
    led_vehicle_parked();

}

void app_driver_barrier_changing(void){
    led_barrier_locking();

}