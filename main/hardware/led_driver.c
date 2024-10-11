#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "app_driver.h"
#include "led_strip.h"
#include "esp_err.h"

static const char *TAG = "led_driver";

// GPIO assignment and LED strip configuration
#define LED_STRIP_BLINK_GPIO CONFIG_LED_GPIO_NUMBER
#define LED_STRIP_LED_NUMBERS 3
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

static led_strip_handle_t led_strip;
static TaskHandle_t led_task_handle = NULL;

// Function to stop the currently running LED task
static void stop_led_task(void) {
    if (led_task_handle != NULL) {
        vTaskDelete(led_task_handle);
        led_task_handle = NULL;
    }
}

// LED Task functions
void led_blink_task(void *param) {
    uint32_t color = (uint32_t)param;
    while (1) {
        for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        vTaskDelay(pdMS_TO_TICKS(500));

        ESP_ERROR_CHECK(led_strip_clear(led_strip));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void led_driver_init(void)
{
    ESP_LOGI(TAG, "Initializing the LED driver.");

    // LED strip general initialization
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,
        .max_leds = LED_STRIP_LED_NUMBERS,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
#else
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .flags.with_dma = false,
#endif
    };

    // Create LED strip object with RMT backend
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED driver object with RMT backend");

    // Ensure the LED strip is off initially
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
}

void led_driver_on(void)
{
    stop_led_task();
    ESP_LOGI(TAG, "Turning the LED ON.");
    for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, 0, 70, 20)); // Example color
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    ESP_LOGI(TAG, "LED Strip turned on");
}

void led_driver_off(void)
{
    stop_led_task();
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
    ESP_LOGI(TAG, "LED Strip turned off");
}

void led_obstacle_warning(void)
{
    stop_led_task();
    ESP_LOGI(TAG, "LED Obstacle Warning.");
    xTaskCreate(led_blink_task, "led_blink_task", 2048, (void*)0xFF0000, 5, &led_task_handle); // Red
}

void led_obstacle_alert(void)
{
    stop_led_task();
    ESP_LOGI(TAG, "LED Obstacle Alert.");
    xTaskCreate(led_blink_task, "led_blink_task", 2048, (void*)0xFFFF00, 5, &led_task_handle); // Yellow
}

void led_vehicle_parked(void)
{
    stop_led_task();
    ESP_LOGI(TAG, "LED Vehicle Parked.");
    for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, 255, 0, 0)); // Green
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}

void led_spot_empty(void)
{
    stop_led_task();
    ESP_LOGI(TAG, "LED Spot Empty");
    xTaskCreate(led_blink_task, "led_blink_task", 2048, (void*)0x00FF00, 5, &led_task_handle); // Green
}

void led_vehicle_leaving(void)
{
    stop_led_task();
    ESP_LOGI(TAG, "LED Vehicle Leaving.");
    xTaskCreate(led_blink_task, "led_blink_task", 2048, (void*)0x0000FF, 5, &led_task_handle); // Blue
}

void led_barrier_locking(void)
{
    stop_led_task();
    ESP_LOGI(TAG, "LED Barrier Locking.");
    xTaskCreate(led_blink_task, "led_blink_task", 2048, (void*)0xFFA500, 5, &led_task_handle); // Orange
}

void led_barrier_unlocking(void)
{
    stop_led_task();
    ESP_LOGI(TAG, "LED Barrier Unlocking.");
    xTaskCreate(led_blink_task, "led_blink_task", 2048, (void*)0xFFA500, 5, &led_task_handle); // Orange
}

void led_connection_lost(void)
{
    stop_led_task();
    ESP_LOGI(TAG, "LED Connection Lost.");
    xTaskCreate(led_blink_task, "led_blink_task", 2048, (void*)0x0000FF, 5, &led_task_handle); // Blue
}

void led_connection_established(void)
{
    stop_led_task();
    ESP_LOGI(TAG, "LED Connection Established.");
    for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, 0, 255, 255)); // Cyan
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}

void led_stop(void)
{
    stop_led_task();
    ESP_LOGI(TAG, "Stopping the LED.");
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
}
