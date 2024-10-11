#ifndef HCSR04_SENSOR_H
#define HCSR04_SENSOR_H

#include <driver/gpio.h>
#include <esp_err.h>

typedef struct {
    gpio_num_t trigger_pin;
    gpio_num_t echo_pin;
} hcsr04_sensor_t;

esp_err_t hcsr04_sensor_init(const hcsr04_sensor_t *dev);
esp_err_t hcsr04_sensor_measure_cm(const hcsr04_sensor_t *dev, uint32_t max_distance_cm, uint32_t *distance_cm);
esp_err_t hcsr04_sensor_measure_cm_temp_compensated(const hcsr04_sensor_t *dev, uint32_t max_distance_cm, uint32_t *distance_cm, float temperature_c);

#endif // HCSR04_SENSOR_H
