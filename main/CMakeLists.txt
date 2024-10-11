set(MAIN_SRCS
    "main.c"
    "communication/mqtt/subscription_manager.c"
    "communication/mqtt/core_mqtt_agent_manager.c"
    "communication/mqtt/core_mqtt_agent_manager_events.c"
    "communication/pppos/pppos_client.c"
    "communication/wifi/app_wifi.c"
    "tasks/pubsub/pubsub.c"
    "tasks/control/control_manager.c"
    "tasks/control/barrier/barrier_control.c"
    "tasks/control/buzzer/buzzer_control.c"
    "tasks/control/led/led_control.c"
    "hardware/app_driver.c"
    "hardware/led_driver.c"
    "hardware/barrier_driver.c"
    "hardware/buzzer_driver.c"
    "hardware/ina3221_sensor.c"
    "hardware/hcsr04_sensor.c"
    "tasks/perception/power/power_perception.c"
    "tasks/perception/obstacle/obstacle_perception.c"
    "tasks/perception/wifi/wifi_perception.c"

)

set(MAIN_INCLUDE_DIRS
    "."
    "communication/mqtt"
    "communication/wifi"
    "communication/pppos"
    "tasks/pubsub"
    "hardware"
    "tasks/control/led"
    "tasks/control/barrier"
    "tasks/control/buzzer"
    "tasks/perception/power"
    "tasks/perception/obstacle"
    "tasks/perception/wifi"

)

set(MAIN_REQUIRES
ultrasonic
led_strip
coreMQTT
coreJSON
coreMQTT-Agent
esp_wifi
esp_timer
backoffAlgorithm
qrcode
wifi_provisioning
unity
driver
esp_modem
esp_netif
esp_event
PRIV_REQUIRES
nvs_flash
mqtt
)

idf_component_register(
    SRCS    
        ${MAIN_SRCS}
    INCLUDE_DIRS
        ${MAIN_INCLUDE_DIRS}
    REQUIRES
        ${MAIN_REQUIRES}
)

# OTA
if( CONFIG_GRI_ENABLE_OTA OR CONFIG_GRI_RUN_QUALIFICATION_TEST )
    target_add_binary_data(${COMPONENT_TARGET} "certs/aws_codesign.crt" TEXT)
endif()

# Root Certificate
target_add_binary_data(${COMPONENT_TARGET} "certs/root_cert_auth.crt" TEXT)
