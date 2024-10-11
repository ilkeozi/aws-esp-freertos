#include <string.h>
#include <stdio.h>
#include <time.h>
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
#include "ina3221_sensor.h"
#include "power_perception.h"

#define CORE_MQTT_AGENT_CONNECTED_BIT (1 << 0)
#define CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT (1 << 1)

static const char *TAG = "power_perception";
extern MQTTAgentContext_t xGlobalMqttAgentContext;
static EventGroupHandle_t xNetworkEventGroup;

static void prvCoreMqttAgentEventHandler(void *pvHandlerArg, esp_event_base_t xEventBase, int32_t lEventId, void *pvEventData);
static void prvPowerPerceptionTask(void *pvParameters);
static void publish_telemetry(ina3221_reading_t readings[3]);

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

static void prvPublishCommandCallback(MQTTAgentCommandContext_t *pxCommandContext, MQTTAgentReturnInfo_t *pxReturnInfo)
{
    if (pxCommandContext != NULL) {
        pxCommandContext->xReturnStatus = pxReturnInfo->returnCode;
        xTaskNotify(pxCommandContext->xTaskToNotify, pxCommandContext->ulNotificationValue, eSetValueWithOverwrite);
    }
}

static BaseType_t prvWaitForCommandAcknowledgment(uint32_t *pulNotifiedValue)
{
    return xTaskNotifyWait(0, 0, pulNotifiedValue, portMAX_DELAY);
}

static void publish_telemetry(ina3221_reading_t readings[3])
{
    char telemetry_topic[128];
    char telemetry_payload[512];
    time_t now;
    time(&now);
    struct tm *timeinfo = gmtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", timeinfo);

    snprintf(telemetry_topic, sizeof(telemetry_topic), "dt/pb/%s/%s/%s/%s/power",
             CONFIG_PB_CITY, CONFIG_PB_AREA, CONFIG_PB_ZONE, CONFIG_GRI_THING_NAME);

    ESP_LOGI(TAG, "Publishing power sensor data to telemetry topic: %s", telemetry_topic);

    snprintf(telemetry_payload, sizeof(telemetry_payload),
             "{\"timestamp\": \"%s\", \"channels\": ["
             "{\"channel\": 1, \"type\": \"regulator\", \"bus_voltage_v\": %.2f, \"shunt_voltage_mv\": %.2f, \"load_voltage_v\": %.2f, \"current_ma\": %.2f},"
             "{\"channel\": 2, \"type\": \"battery\", \"bus_voltage_v\": %.2f, \"shunt_voltage_mv\": %.2f, \"load_voltage_v\": %.2f, \"current_ma\": %.2f},"
             "{\"channel\": 3, \"type\": \"motor\", \"bus_voltage_v\": %.2f, \"shunt_voltage_mv\": %.2f, \"load_voltage_v\": %.2f, \"current_ma\": %.2f}"
             "], \"session-id\": \"session-987654321\", \"status\": \"ok\"}",
             timestamp,
             readings[0].bus_voltage, readings[0].shunt_voltage, readings[0].load_voltage, readings[0].current,
             readings[1].bus_voltage, readings[1].shunt_voltage, readings[1].load_voltage, readings[1].current,
             readings[2].bus_voltage, readings[2].shunt_voltage, readings[2].load_voltage, readings[2].current);


    MQTTStatus_t status;
    MQTTPublishInfo_t publishInfo = {
        .qos = MQTTQoS1,
        .pTopicName = telemetry_topic,
        .topicNameLength = (uint16_t)strlen(telemetry_topic),
        .pPayload = telemetry_payload,
        .payloadLength = strlen(telemetry_payload),
        .retain = false,
        .dup = false
    };

    MQTTAgentCommandContext_t xCommandContext = {0};
    xCommandContext.xTaskToNotify = xTaskGetCurrentTaskHandle();
    xCommandContext.ulNotificationValue = 1;

    MQTTAgentCommandInfo_t commandInfo = {
        .cmdCompleteCallback = prvPublishCommandCallback,
        .pCmdCompleteCallbackContext = &xCommandContext,
        .blockTimeMs = 1000
    };

    if (publishInfo.pTopicName == NULL || publishInfo.topicNameLength == 0) {
        ESP_LOGE(TAG, "Invalid topic name");
    }

    if (publishInfo.pPayload == NULL || publishInfo.payloadLength == 0) {
        ESP_LOGE(TAG, "Invalid payload");
    }

    status = MQTTAgent_Publish(&xGlobalMqttAgentContext, &publishInfo, &commandInfo);
    if (status != MQTTSuccess) {
        ESP_LOGE(TAG, "Failed to publish telemetry: %s", MQTT_Status_strerror(status));
        return;
    }

    uint32_t ulNotifiedValue;
    if (prvWaitForCommandAcknowledgment(&ulNotifiedValue) == pdTRUE && ulNotifiedValue == 1 && xCommandContext.xReturnStatus == MQTTSuccess) {
        ESP_LOGI(TAG, "Telemetry publish acknowledged");
    } else {
        ESP_LOGE(TAG, "Telemetry publish failed or not acknowledged");
    }
}

static void prvPowerPerceptionTask(void *pvParameters)
{
    ina3221_init();
    ina3221_reading_t readings[3];

    while (1) {
        for (uint8_t channel = 1; channel <= 3; channel++) {
            if (ina3221_read_channel(channel, &readings[channel - 1]) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to read channel %d", channel);
            }
        }

        publish_telemetry(readings);

        vTaskDelay(pdMS_TO_TICKS(180000)); // Publish every 30 seconds
    }
}

void vStartPowerPerception(void)
{
    xTaskCreate(prvPowerPerceptionTask, "PowerPerception", 4096, NULL, 5, NULL);
}
