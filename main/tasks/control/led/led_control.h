#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_err.h>

/**
 * @brief Initialize and start the LED control task.
 *
 * This function initializes necessary hardware and MQTT subscriptions,
 * then starts a FreeRTOS task to handle incoming MQTT messages for LED control.
 */
void vStartLEDControl(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_CONTROL_H */
