#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "sdkconfig.h"

#define BUZZER_GPIO 16
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_DUTY_RES       LEDC_TIMER_13_BIT  // Set duty resolution to 13 bits
#define LEDC_DUTY           (8192)             // Set duty to 50%
#define LEDC_FREQUENCY      (5000)             // Frequency in Hertz. Set frequency at 5 kHz

static const char *TAG = "buzzer_driver";

// Frequencies of musical notes (in Hertz)
#define NOTE_C4  261
#define NOTE_D4  294
#define NOTE_E4  329
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  493
#define NOTE_C5  523

void buzzer_driver_init(void)
{
    ESP_LOGI(TAG, "Initializing the buzzer driver.");

    // Prepare and then apply the LEDC timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // Prepare and then apply the LEDC channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = BUZZER_GPIO,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

void buzzer_play_note(int frequency, int duration)
{
    ledc_set_freq(LEDC_MODE, LEDC_TIMER, frequency);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

    vTaskDelay(pdMS_TO_TICKS(duration));

    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void buzzer_obstacle_warning(void)
{
    ESP_LOGI(TAG, "Playing obstacle warning sound.");
    buzzer_play_note(NOTE_C4, 200);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_play_note(NOTE_C4, 200);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_play_note(NOTE_C4, 200);
}

void buzzer_obstacle_alert(void)
{
    ESP_LOGI(TAG, "Playing obstacle alert sound.");
    buzzer_play_note(NOTE_F4, 500);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_play_note(NOTE_F4, 500);
}

void buzzer_vehicle_parked(void)
{
    ESP_LOGI(TAG, "Playing vehicle parked sound.");
    buzzer_play_note(NOTE_G4, 300);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_play_note(NOTE_E4, 300);
}

void buzzer_vehicle_leaving(void)
{
    ESP_LOGI(TAG, "Playing vehicle leaving sound.");
    buzzer_play_note(NOTE_A4, 300);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_play_note(NOTE_C5, 300);
}

void buzzer_barrier_locking(void)
{
    ESP_LOGI(TAG, "Playing barrier locking sound.");
    buzzer_play_note(NOTE_B4, 200);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_play_note(NOTE_B4, 200);
}

void buzzer_barrier_unlocking(void)
{
    ESP_LOGI(TAG, "Playing barrier unlocking sound.");
    buzzer_play_note(NOTE_C5, 300);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_play_note(NOTE_C5, 300);
}

void buzzer_connection_lost(void)
{
    ESP_LOGI(TAG, "Playing connection lost sound.");
    buzzer_play_note(NOTE_D4, 400);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_play_note(NOTE_D4, 400);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_play_note(NOTE_D4, 400);
}

void buzzer_connection_established(void)
{
    ESP_LOGI(TAG, "Playing connection established sound.");
    buzzer_play_note(NOTE_E4, 300);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_play_note(NOTE_G4, 300);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_play_note(NOTE_E4, 300);
}

void buzzer_stop(void)
{
    ESP_LOGI(TAG, "Stopping the buzzer.");
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}
