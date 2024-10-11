#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/mcpwm.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "barrier_driver.h"
#include "app_driver.h"
#include "esp_timer.h"
#include "esp_err.h"

static const char *TAG = "barrier_driver";

// GPIO assignment for L298N motor driver and limit switches
#define L298N_IN1_GPIO CONFIG_PB_L298N_IN1_GPIO
#define L298N_IN2_GPIO CONFIG_PB_L298N_IN2_GPIO
#define LOCKED_LIMIT_SWITCH_GPIO CONFIG_PB_LOCKED_LIMIT_SWITCH_GPIO
#define UNLOCKED_LIMIT_SWITCH_GPIO CONFIG_PB_UNLOCKED_LIMIT_SWITCH_GPIO
#define FC33_SENSOR_GPIO 15 

#define BARRIER_OPERATION_TIMEOUT_MS 15000  // Timeout period in milliseconds

static void stop_motor(void)
{
    mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
    mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);
}

void barrier_driver_init(void)
{
    ESP_LOGI(TAG, "Initializing the barrier driver.");

    // Initialize MCPWM
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, L298N_IN1_GPIO);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, L298N_IN2_GPIO);

    mcpwm_config_t pwm_config;
    pwm_config.frequency = 1000;  // Frequency in Hz
    pwm_config.cmpr_a = 0;        // Duty cycle of PWMxA = 0
    pwm_config.cmpr_b = 0;        // Duty cycle of PWMxB = 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;

    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);

    // Configure limit switches GPIOs as input with pull-up
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << FC33_SENSOR_GPIO);  // Single FC-33 sensor
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;  // Enable pull-up to avoid floating states
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);
}

void barrier_driver_open(void)
{
    ESP_LOGI(TAG, "Opening the barrier.");

    // Activate the motor to open the barrier
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 100.0);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 0.0);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);

    // Wait until the unlocked limit switch is triggered or timeout occurs
    app_driver_barrier_changing();

    const int64_t grace_period_us = 500000;  // 500ms grace period
    int64_t start_time = esp_timer_get_time();

    // Wait for the grace period to expire
    while ((esp_timer_get_time() - start_time) < grace_period_us)
    {
        vTaskDelay(pdMS_TO_TICKS(10));  // Short delay to yield to other tasks
    }

    // After the grace period, start checking the FC-33 sensor
    start_time = esp_timer_get_time();
    while (gpio_get_level(FC33_SENSOR_GPIO) == 0)
    {
        if ((esp_timer_get_time() - start_time) > BARRIER_OPERATION_TIMEOUT_MS * 1000)
        {
            ESP_LOGE(TAG, "Timeout occurred while opening the barrier.");
            stop_motor();
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Stop the motor
    stop_motor();
    app_driver_led_unlocked();
    ESP_LOGI(TAG, "Barrier opened.");
}

void barrier_driver_close(void)
{
    ESP_LOGI(TAG, "Closing the barrier.");

    // Activate the motor to close the barrier
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 0.0);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 100.0);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
    app_driver_barrier_changing();

    const int64_t grace_period_us = 500000;  // 500ms grace period
    int64_t start_time = esp_timer_get_time();

    // Wait for the grace period to expire
    while ((esp_timer_get_time() - start_time) < grace_period_us)
    {
        vTaskDelay(pdMS_TO_TICKS(10));  // Short delay to yield to other tasks
    }

    // After the grace period, start checking the FC-33 sensor
    start_time = esp_timer_get_time();
    // Wait until the locked limit switch is triggered or timeout occurs
    while (gpio_get_level(FC33_SENSOR_GPIO) == 0)
    {
        if ((esp_timer_get_time() - start_time) > BARRIER_OPERATION_TIMEOUT_MS * 1000)
        {
            ESP_LOGE(TAG, "Timeout occurred while closing the barrier.");
            stop_motor();
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Stop the motor
    stop_motor();
    app_driver_led_locked();
    ESP_LOGI(TAG, "Barrier closed.");
}

void barrier_driver_test_limit_switches(void)
{
    ESP_LOGI(TAG, "Testing limit switches.");

    while (1)
    {
        int locked_switch_state = gpio_get_level(LOCKED_LIMIT_SWITCH_GPIO);
        int unlocked_switch_state = gpio_get_level(UNLOCKED_LIMIT_SWITCH_GPIO);

        ESP_LOGI(TAG, "Locked limit switch state: %d", locked_switch_state);
        ESP_LOGI(TAG, "Unlocked limit switch state: %d", unlocked_switch_state);

        vTaskDelay(pdMS_TO_TICKS(1000));  // Delay for 1 second between checks
    }
}

void barrier_driver_test_fc33_sensor(void)
{
    ESP_LOGI(TAG, "Testing FC-33 sensor.");

    while (1)
    {
        // Read the current state of the FC-33 sensor (LOW when object detected, HIGH otherwise)
        int fc33_sensor_state = gpio_get_level(FC33_SENSOR_GPIO);

        // Log the current state of the FC-33 sensor
        ESP_LOGI(TAG, "FC-33 sensor state: %d", fc33_sensor_state);

        // Delay for 1 second between checks
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}