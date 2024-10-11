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
#include "barrier_control.h"
#include "barrier_control_config.h"

#define CORE_MQTT_AGENT_CONNECTED_BIT              ( 1 << 0 )
#define CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT    ( 1 << 1 )

struct MQTTAgentCommandContext
{
    MQTTStatus_t xReturnStatus;
    TaskHandle_t xTaskToNotify;
    uint32_t ulNotificationValue;
    void * pArgs;
};

static const char * TAG = "barrier_control";
extern MQTTAgentContext_t xGlobalMqttAgentContext;
static char unlockTopicBuf[BARRIER_CONTROL_STRING_BUFFER_LENGTH];
static char lockTopicBuf[BARRIER_CONTROL_STRING_BUFFER_LENGTH];
static EventGroupHandle_t xNetworkEventGroup;

static void prvCoreMqttAgentEventHandler(void * pvHandlerArg, esp_event_base_t xEventBase, int32_t lEventId, void * pvEventData);
static void prvSubscribeCommandCallback(MQTTAgentCommandContext_t * pxCommandContext, MQTTAgentReturnInfo_t * pxReturnInfo);
static BaseType_t prvWaitForCommandAcknowledgment(uint32_t * pulNotifiedValue);
static void prvIncomingPublishCallback(void * pvIncomingPublishCallbackContext, MQTTPublishInfo_t * pxPublishInfo);
static bool prvSubscribeToTopic(MQTTQoS_t xQoS, char * pcTopicFilter);
static void prvBarrierControlTask(void * pvParameters);

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
    char command[32] = {0};

    ESP_LOGI(TAG, "Parsing incoming publish: %.*s", publishPayloadLength, publishPayload);

    result = JSON_Validate(publishPayload, publishPayloadLength);

    if (result == JSONSuccess)
    {
        ESP_LOGI(TAG, "JSON is valid");

        result = JSON_Search(publishPayload, publishPayloadLength, "command", strlen("command"), &outValue, &outValueLength);

        if (result == JSONSuccess)
        {
            ESP_LOGI(TAG, "Found 'command' key");

            // Ensure the extracted value is null-terminated for safe usage
            strncpy(command, outValue, outValueLength);
            command[outValueLength] = '\0';

            if (strcmp(command, "unlock") == 0)
            {
                ESP_LOGI(TAG, "Unlocking barrier");
                app_driver_barrier_open();
            }
            else if (strcmp(command, "lock") == 0)
            {
                ESP_LOGI(TAG, "Locking barrier");
                app_driver_barrier_close();
            }
            else
            {
                ESP_LOGE(TAG, "Unknown command: %s", command);
            }
        }
        else
        {
            ESP_LOGI(TAG, "No 'command' key found in the JSON payload");
        }
    }
    else
    {
        ESP_LOGE(TAG, "The JSON document is invalid!");
    }
}

static void prvIncomingPublishCallback(void * pvIncomingPublishCallbackContext, MQTTPublishInfo_t * pxPublishInfo)
{
    static char cTerminatedString[BARRIER_CONTROL_STRING_BUFFER_LENGTH];

    (void) pvIncomingPublishCallbackContext;

    if(pxPublishInfo->payloadLength < BARRIER_CONTROL_STRING_BUFFER_LENGTH)
    {
        memcpy((void *) cTerminatedString, pxPublishInfo->pPayload, pxPublishInfo->payloadLength);
        cTerminatedString[pxPublishInfo->payloadLength] = 0x00;
    }
    else
    {
        memcpy((void *) cTerminatedString, pxPublishInfo->pPayload, BARRIER_CONTROL_STRING_BUFFER_LENGTH);
        cTerminatedString[BARRIER_CONTROL_STRING_BUFFER_LENGTH - 1] = 0x00;
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

    xCommandParams.blockTimeMs = BARRIER_CONTROL_CONFIG_MAX_COMMAND_SEND_BLOCK_TIME_MS;
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
        app_driver_led_connected();
        ESP_LOGI(TAG, "Received subscribe ack for topic %s containing ID %d", pcTopicFilter, (int) xApplicationDefinedContext.ulNotificationValue);
    }

    return xCommandAcknowledged;
}

static void prvBarrierControlTask(void * pvParameters)
{
    const char * pcTaskName;

    pcTaskName = pcTaskGetName(xTaskGetCurrentTaskHandle());

    app_driver_init();
    app_driver_led_disconnected();

    xNetworkEventGroup = xEventGroupCreate();
    xEventGroupSetBits(xNetworkEventGroup, CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT);

    xCoreMqttAgentManagerRegisterHandler(prvCoreMqttAgentEventHandler);

    MQTTQoS_t xQoS = (MQTTQoS_t) BARRIER_CONTROL_CONFIG_QOS_LEVEL;

    snprintf(unlockTopicBuf, BARRIER_CONTROL_STRING_BUFFER_LENGTH, "cmd/pb/%s/%s/%s/%s/barrier/unlock", CONFIG_PB_CITY, CONFIG_PB_AREA, CONFIG_PB_ZONE, CONFIG_GRI_THING_NAME);
    snprintf(lockTopicBuf, BARRIER_CONTROL_STRING_BUFFER_LENGTH, "cmd/pb/%s/%s/%s/%s/barrier/lock", CONFIG_PB_CITY, CONFIG_PB_AREA, CONFIG_PB_ZONE, CONFIG_GRI_THING_NAME);

    if(!prvSubscribeToTopic(xQoS, unlockTopicBuf))
    {
        ESP_LOGE(TAG, "Failed to subscribe to topic %s", unlockTopicBuf);
        vTaskDelete(NULL);
    }

    if(!prvSubscribeToTopic(xQoS, lockTopicBuf))
    {
        ESP_LOGE(TAG, "Failed to subscribe to topic %s", lockTopicBuf);
        vTaskDelete(NULL);
    }

    while(1)
    {
        xEventGroupWaitBits(xNetworkEventGroup, CORE_MQTT_AGENT_CONNECTED_BIT | CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(BARRIER_CONTROL_CONFIG_DELAY_BETWEEN_PUBLISH_OPERATIONS_MS));
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
            app_driver_led_connected();
            break;

        case CORE_MQTT_AGENT_DISCONNECTED_EVENT:
            ESP_LOGI(TAG, "coreMQTT-Agent disconnected. Preventing coreMQTT-Agent commands from being enqueued.");
            app_driver_led_disconnected();
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

void vStartBarrierControl(void)
{
    xTaskCreate(prvBarrierControlTask, "BarrierControl", BARRIER_CONTROL_CONFIG_TASK_STACK_SIZE, NULL, BARRIER_CONTROL_CONFIG_TASK_PRIORITY, NULL);
}
