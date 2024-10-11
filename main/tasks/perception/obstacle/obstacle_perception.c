#include <string.h>
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "sdkconfig.h"
#include "core_mqtt.h"
#include "core_mqtt_agent.h"
#include "core_mqtt_agent_manager.h"
#include "core_mqtt_agent_manager_events.h"
#include "hcsr04_sensor.h"
#include "driver/gpio.h"
#include "obstacle_perception.h"

#define CORE_MQTT_AGENT_CONNECTED_BIT (1 << 0)
#define CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT (1 << 1)

#define TRIGGER_PIN 5
#define ECHO_PIN 17

#define LOCKED_LIMIT_SWITCH_GPIO CONFIG_PB_LOCKED_LIMIT_SWITCH_GPIO
#define UNLOCKED_LIMIT_SWITCH_GPIO CONFIG_PB_UNLOCKED_LIMIT_SWITCH_GPIO

static const char *TAG = "obstacle_perception";
extern MQTTAgentContext_t xGlobalMqttAgentContext;
static EventGroupHandle_t xNetworkEventGroup;
static hcsr04_sensor_t sensor = {
    .trigger_pin = TRIGGER_PIN,
    .echo_pin = ECHO_PIN
};

static void prvCoreMqttAgentEventHandler(void *pvHandlerArg, esp_event_base_t xEventBase, int32_t lEventId, void *pvEventData);
static void prvObstaclePerceptionTask(void *pvParameters);

typedef struct MQTTAgentCommandContext
{
    MQTTStatus_t xReturnStatus;
    TaskHandle_t xTaskToNotify;
    uint32_t ulNotificationValue;
    void *pArgs;
} MQTTAgentCommandContext_t;

static void prvCoreMqttAgentEventHandler(void *pvHandlerArg, esp_event_base_t xEventBase, int32_t lEventId, void *pvEventData)
{
    (void)pvHandlerArg;
    (void)xEventBase;
    (void)pvEventData;

    switch (lEventId) {
        case CORE_MQTT_AGENT_CONNECTED_EVENT:
            ESP_LOGI(TAG, "coreMQTT-Agent connected.");
            xEventGroupSetBits(xNetworkEventGroup, CORE_MQTT_AGENT_CONNECTED_BIT);
            break;

        case CORE_MQTT_AGENT_DISCONNECTED_EVENT:
            ESP_LOGI(TAG, "coreMQTT-Agent disconnected. Preventing coreMQTT-Agent commands from being enqueued.");
            xEventGroupClearBits(xNetworkEventGroup, CORE_MQTT_AGENT_CONNECTED_BIT);
            break;

        case CORE_MQTT_AGENT_OTA_STARTED_EVENT:
            ESP_LOGI(TAG, "OTA started. Preventing coreMQTT-Agent commands from being enqueued.");
            xEventGroupClearBits(xNetworkEventGroup, CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT);
            break;

        case CORE_MQTT_AGENT_OTA_STOPPED_EVENT:
            ESP_LOGI(TAG, "OTA stopped. No longer preventing coreMQTT-Agent commands from being enqueued.");
            xEventGroupSetBits(xNetworkEventGroup, CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT);
            break;

        default:
            ESP_LOGE(TAG, "coreMQTT-Agent event handler received unexpected event: %" PRIu32 "", lEventId);
            break;
    }
}

static void prvObstaclePerceptionTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Initializing obstacle perception task");

    hcsr04_sensor_init(&sensor);

    while (1) {
        uint32_t distance_cm;

        esp_err_t result = hcsr04_sensor_measure_cm(&sensor, 500, &distance_cm);

        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to measure distance: %s", esp_err_to_name(result));
        }

        // Check the status of the limit switches
        if (gpio_get_level(LOCKED_LIMIT_SWITCH_GPIO) == 0) {
            ESP_LOGI(TAG, "Obstacle detected: Distance = %" PRIu32 " cm", distance_cm);
            // Publish an obstacle detected event
            // Add your MQTT publish logic here if needed
        }

        if (gpio_get_level(UNLOCKED_LIMIT_SWITCH_GPIO) == 1) {
            ESP_LOGI(TAG, "Vehicle detected: Distance = %" PRIu32 " cm", distance_cm);
            // Publish a vehicle detected event
            // Add your MQTT publish logic here if needed
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Measure every 1 second
    }
}

void ultrasonic_test_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Initializing ultrasonic sensor");

    esp_err_t init_result = hcsr04_sensor_init(&sensor);
    if (init_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ultrasonic sensor: %s", esp_err_to_name(init_result));
        vTaskDelete(NULL);
    }

    while (1) {
        uint32_t distance_cm;

        esp_err_t result = hcsr04_sensor_measure_cm(&sensor, 100, &distance_cm); // Start with a max distance of 100 cm

        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to measure distance: %s (0x%x)", esp_err_to_name(result), result);
        }

        // Check the status of the limit switches
        if (gpio_get_level(LOCKED_LIMIT_SWITCH_GPIO) == 1) {
            ESP_LOGI(TAG, "Obstacle detected: Distance = %" PRIu32 " cm", distance_cm);
            // Publish an obstacle detected event
            // Add your MQTT publish logic here if needed
        }

        if (gpio_get_level(UNLOCKED_LIMIT_SWITCH_GPIO) == 1) {
            ESP_LOGI(TAG, "Vehicle detected: Distance = %" PRIu32 " cm", distance_cm);
            // Publish a vehicle detected event
            // Add your MQTT publish logic here if needed
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Measure every 1 second
    }
}

void vStartObstaclePerception(void)
{
    xTaskCreate(ultrasonic_test_task, "ObstaclePerception", 4096, NULL, 5, NULL);
}
