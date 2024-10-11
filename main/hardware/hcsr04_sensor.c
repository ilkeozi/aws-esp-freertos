#include <inttypes.h>
#include <stdio.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include "hcsr04_sensor.h"
#include "esp_log.h"

#define TRIGGER_LOW_DELAY_US 2
#define TRIGGER_HIGH_DELAY_US 10
#define PING_TIMEOUT_US 60000
#define SPEED_OF_SOUND_CM_PER_US_AT_0C 0.0343 // Speed of sound in cm/us at 0 degrees Celsius

#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)
#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)

static const char *TAG = "HCSR04_SENSOR";

static inline bool timeout_expired(int64_t start, int64_t timeout) {
    return (esp_timer_get_time() - start) >= timeout;
}

esp_err_t hcsr04_sensor_init(const hcsr04_sensor_t *dev)
{
    CHECK_ARG(dev);

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << dev->trigger_pin),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << dev->echo_pin);
    gpio_config(&io_conf);

    gpio_set_level(dev->trigger_pin, 0);

    ESP_LOGI(TAG, "HC-SR04 sensor initialized with trigger pin: %d, echo pin: %d", dev->trigger_pin, dev->echo_pin);
    return ESP_OK;
}

esp_err_t hcsr04_sensor_measure_raw(const hcsr04_sensor_t *dev, uint32_t max_time_us, uint32_t *time_us)
{
    CHECK_ARG(dev && time_us);

    gpio_set_level(dev->trigger_pin, 0);
    esp_rom_delay_us(TRIGGER_LOW_DELAY_US);
    gpio_set_level(dev->trigger_pin, 1);
    esp_rom_delay_us(TRIGGER_HIGH_DELAY_US);
    gpio_set_level(dev->trigger_pin, 0);

    int64_t start_time = esp_timer_get_time();
    while (gpio_get_level(dev->echo_pin) == 0) {
        if (timeout_expired(start_time, PING_TIMEOUT_US)) {
            return ESP_ERR_TIMEOUT;
        }
    }

    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(dev->echo_pin) == 1) {
        if (timeout_expired(echo_start, max_time_us)) {
            // ESP_LOGE(TAG, "Echo timeout");
        }
    }
    int64_t echo_end = esp_timer_get_time();

    *time_us = echo_end - echo_start;

    return ESP_OK;
}

esp_err_t hcsr04_sensor_measure_cm(const hcsr04_sensor_t *dev, uint32_t max_distance_cm, uint32_t *distance_cm)
{
    CHECK_ARG(dev && distance_cm);

    uint32_t time_us;
    esp_err_t res = hcsr04_sensor_measure_raw(dev, max_distance_cm * 58, &time_us);
    if (res != ESP_OK) {
        return res;
    }

    *distance_cm = time_us / 58;

    return ESP_OK;
}

esp_err_t hcsr04_sensor_measure_cm_temp_compensated(const hcsr04_sensor_t *dev, uint32_t max_distance_cm, uint32_t *distance_cm, float temperature_c)
{
    CHECK_ARG(dev && distance_cm);

    float speed_of_sound_cm_us = SPEED_OF_SOUND_CM_PER_US_AT_0C + (0.0006 * temperature_c);
    uint32_t max_time_us = (uint32_t)(max_distance_cm / speed_of_sound_cm_us);
    
    uint32_t time_us;
    esp_err_t res = hcsr04_sensor_measure_raw(dev, max_time_us, &time_us);
    if (res != ESP_OK) {
        return res;
    }

    *distance_cm = (uint32_t)(time_us * speed_of_sound_cm_us);

    return ESP_OK;
}
