#include <string.h>
#include <stdio.h>
#include <assert.h>
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
#include "core_json.h"
#include "subscription_manager.h"
#include "app_driver.h"
#include "buzzer_control.h"
#include "buzzer_control_config.h"
#include "buzzer_driver.h"

#define CORE_MQTT_AGENT_CONNECTED_BIT              ( 1 << 0 )
#define CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT    ( 1 << 1 )

struct MQTTAgentCommandContext
{
    MQTTStatus_t xReturnStatus;
    TaskHandle_t xTaskToNotify;
    uint32_t ulNotificationValue;
    void * pArgs;
};

static const char * TAG = "buzzer_control";
extern MQTTAgentContext_t xGlobalMqttAgentContext;
static char buzzerTopicBuf[BUZZER_CONTROL_STRING_BUFFER_LENGTH];
static EventGroupHandle_t xNetworkEventGroup;

static void prvCoreMqttAgentEventHandler(void * pvHandlerArg, esp_event_base_t xEventBase, int32_t lEventId, void * pvEventData);
static void prvSubscribeCommandCallback(MQTTAgentCommandContext_t * pxCommandContext, MQTTAgentReturnInfo_t * pxReturnInfo);
static BaseType_t prvWaitForCommandAcknowledgment(uint32_t * pulNotifiedValue);
static void prvIncomingPublishCallback(void * pvIncomingPublishCallbackContext, MQTTPublishInfo_t * pxPublishInfo);
static bool prvSubscribeToTopic(MQTTQoS_t xQoS, char * pcTopicFilter);
static void prvBuzzerControlTask(void * pvParameters);

static void prvSubscribeCommandCallback(MQTTAgentCommandContext_t * pxCommandContext, MQTTAgentReturnInfo_t * pxReturnInfo)
{
    bool xSubscriptionAdded = false;
    MQTTAgentSubscribeArgs_t * pxSubscribeArgs = (MQTTAgentSubscribeArgs_t *) pxCommandContext->pArgs;

    pxCommandContext->xReturnStatus = pxReturnInfo->returnCode;

    if(pxReturnInfo->returnCode == MQTTSuccess)
    {
        xSubscriptionAdded = addSubscription((SubscriptionElement_t *) xGlobalMqttAgentContext.pIncomingCallbackContext,
                                              pxSubscribeArgs->pSubscribeInfo->pTopicFilter,
                                              pxSubscribeArgs->pSubscribeInfo->topicFilterLength,
                                              prvIncomingPublishCallback,
                                              NULL);

        if(!xSubscriptionAdded)
        {
            ESP_LOGE(TAG, "Failed to register an incoming publish callback for topic %.*s.",
                     pxSubscribeArgs->pSubscribeInfo->topicFilterLength,
                     pxSubscribeArgs->pSubscribeInfo->pTopicFilter);
        }
    }

    xTaskNotify(pxCommandContext->xTaskToNotify, (uint32_t) (pxReturnInfo->returnCode), eSetValueWithOverwrite);
}

static BaseType_t prvWaitForCommandAcknowledgment(uint32_t * pulNotifiedValue)
{
    return xTaskNotifyWait(0, 0, pulNotifiedValue, portMAX_DELAY);
}

static void prvParseIncomingPublish(char * publishPayload, size_t publishPayloadLength)
{
    char * outValue = NULL;
    size_t outValueLength = 0;
    JSONStatus_t result = JSONSuccess;
    char type[32] = {0};

    ESP_LOGI(TAG, "Parsing incoming publish: %.*s", publishPayloadLength, publishPayload);

    result = JSON_Validate(publishPayload, publishPayloadLength);

    if (result == JSONSuccess)
    {
        ESP_LOGI(TAG, "JSON is valid");

        result = JSON_Search(publishPayload, publishPayloadLength, "type", strlen("type"), &outValue, &outValueLength);

        if (result == JSONSuccess)
        {
            ESP_LOGI(TAG, "Found 'type' key");

            // Ensure the extracted value is null-terminated for safe usage
            strncpy(type, outValue, outValueLength);
            type[outValueLength] = '\0';

            if (strcmp(type, "obstacle-warning") == 0)
            {
                ESP_LOGI(TAG, "Playing obstacle warning sound");
                buzzer_obstacle_warning();
            }
            else if (strcmp(type, "obstacle-alert") == 0)
            {
                ESP_LOGI(TAG, "Playing obstacle alert sound");
                buzzer_obstacle_alert();
            }
            else if (strcmp(type, "vehicle-parked") == 0)
            {
                ESP_LOGI(TAG, "Playing vehicle parked sound");
                buzzer_vehicle_parked();
            }
            else if (strcmp(type, "vehicle-leaving") == 0)
            {
                ESP_LOGI(TAG, "Playing vehicle leaving sound");
                buzzer_vehicle_leaving();
            }
            else if (strcmp(type, "barrier-locking") == 0)
            {
                ESP_LOGI(TAG, "Playing barrier locking sound");
                buzzer_barrier_locking();
            }
            else if (strcmp(type, "barrier-unlocking") == 0)
            {
                ESP_LOGI(TAG, "Playing barrier unlocking sound");
                buzzer_barrier_unlocking();
            }
            else if (strcmp(type, "connection-lost") == 0)
            {
                ESP_LOGI(TAG, "Playing connection lost sound");
                buzzer_connection_lost();
            }
            else if (strcmp(type, "connection-established") == 0)
            {
                ESP_LOGI(TAG, "Playing connection established sound");
                buzzer_connection_established();
            }
            else if (strcmp(type, "stop") == 0)
            {
                ESP_LOGI(TAG, "Stopping buzzer");
                buzzer_stop();
            }
            else
            {
                ESP_LOGE(TAG, "Unknown type: %s", type);
            }
        }
        else
        {
            ESP_LOGI(TAG, "No 'type' key found in the JSON payload");
        }
    }
    else
    {
        ESP_LOGE(TAG, "The JSON document is invalid!");
    }
}

static void prvIncomingPublishCallback(void * pvIncomingPublishCallbackContext, MQTTPublishInfo_t * pxPublishInfo)
{
    static char cTerminatedString[BUZZER_CONTROL_STRING_BUFFER_LENGTH];

    (void) pvIncomingPublishCallbackContext;

    if(pxPublishInfo->payloadLength < BUZZER_CONTROL_STRING_BUFFER_LENGTH)
    {
        memcpy((void *) cTerminatedString, pxPublishInfo->pPayload, pxPublishInfo->payloadLength);
        cTerminatedString[pxPublishInfo->payloadLength] = 0x00;
    }
    else
    {
        memcpy((void *) cTerminatedString, pxPublishInfo->pPayload, BUZZER_CONTROL_STRING_BUFFER_LENGTH);
        cTerminatedString[BUZZER_CONTROL_STRING_BUFFER_LENGTH - 1] = 0x00;
    }

    ESP_LOGI(TAG, "Received incoming publish message %s", cTerminatedString);

    prvParseIncomingPublish((char *) pxPublishInfo->pPayload, pxPublishInfo->payloadLength);
}

static bool prvSubscribeToTopic(MQTTQoS_t xQoS, char * pcTopicFilter)
{
    MQTTStatus_t xCommandAdded;
    BaseType_t xCommandAcknowledged = pdFALSE;
    MQTTAgentSubscribeArgs_t xSubscribeArgs;
    MQTTSubscribeInfo_t xSubscribeInfo;
    static int32_t ulNextSubscribeMessageID = 0;
    MQTTAgentCommandContext_t xApplicationDefinedContext = {0UL};
    MQTTAgentCommandInfo_t xCommandParams = {0UL};

    xTaskNotifyStateClear(NULL);

    ulNextSubscribeMessageID++;

    xSubscribeInfo.pTopicFilter = pcTopicFilter;
    xSubscribeInfo.topicFilterLength = (uint16_t) strlen(pcTopicFilter);
    xSubscribeInfo.qos = xQoS;
    xSubscribeArgs.pSubscribeInfo = &xSubscribeInfo;
    xSubscribeArgs.numSubscriptions = 1;

    xApplicationDefinedContext.ulNotificationValue = ulNextSubscribeMessageID;
    xApplicationDefinedContext.xTaskToNotify = xTaskGetCurrentTaskHandle();
    xApplicationDefinedContext.pArgs = (void *) &xSubscribeArgs;

    xCommandParams.blockTimeMs = BUZZER_CONTROL_CONFIG_MAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvSubscribeCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = (void *) &xApplicationDefinedContext;

    ESP_LOGI(TAG, "Sending subscribe request to agent for topic filter: %s with id %d", pcTopicFilter, (int) ulNextSubscribeMessageID);

    do
    {
        xCommandAdded = MQTTAgent_Subscribe(&xGlobalMqttAgentContext, &xSubscribeArgs, &xCommandParams);
    } while(xCommandAdded != MQTTSuccess);

    xCommandAcknowledged = prvWaitForCommandAcknowledgment(NULL);

    if((xCommandAcknowledged != pdTRUE) || (xApplicationDefinedContext.xReturnStatus != MQTTSuccess))
    {
        ESP_LOGE(TAG, "Error or timed out waiting for ack to subscribe message topic %s", pcTopicFilter);
    }
    else
    {
        ESP_LOGI(TAG, "Received subscribe ack for topic %s containing ID %d", pcTopicFilter, (int) xApplicationDefinedContext.ulNotificationValue);
    }

    return xCommandAcknowledged;
}

static void prvBuzzerControlTask(void * pvParameters)
{
    const char * pcTaskName;

    pcTaskName = pcTaskGetName(xTaskGetCurrentTaskHandle());

    buzzer_driver_init();

    xNetworkEventGroup = xEventGroupCreate();
    xEventGroupSetBits(xNetworkEventGroup, CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT);

    xCoreMqttAgentManagerRegisterHandler(prvCoreMqttAgentEventHandler);

    MQTTQoS_t xQoS = (MQTTQoS_t) BUZZER_CONTROL_CONFIG_QOS_LEVEL;

    snprintf(buzzerTopicBuf, BUZZER_CONTROL_STRING_BUFFER_LENGTH, "cmd/pb/%s/%s/%s/%s/buzzer", CONFIG_PB_CITY, CONFIG_PB_AREA, CONFIG_PB_ZONE, CONFIG_GRI_THING_NAME);

    if(!prvSubscribeToTopic(xQoS, buzzerTopicBuf))
    {
        ESP_LOGE(TAG, "Failed to subscribe to topic %s", buzzerTopicBuf);
        vTaskDelete(NULL);
    }

    while(1)
    {
        xEventGroupWaitBits(xNetworkEventGroup, CORE_MQTT_AGENT_CONNECTED_BIT | CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(BUZZER_CONTROL_CONFIG_DELAY_BETWEEN_PUBLISH_OPERATIONS_MS));
    }

    vTaskDelete(NULL);
}

static void prvCoreMqttAgentEventHandler(void * pvHandlerArg, esp_event_base_t xEventBase, int32_t lEventId, void * pvEventData)
{
    (void) pvHandlerArg;
    (void) xEventBase;
    (void) pvEventData;

    switch(lEventId)
    {
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

void vStartBuzzerControl(void)
{
    xTaskCreate(prvBuzzerControlTask, "BuzzerControl", BUZZER_CONTROL_CONFIG_TASK_STACK_SIZE, NULL, BUZZER_CONTROL_CONFIG_TASK_PRIORITY, NULL);
}
