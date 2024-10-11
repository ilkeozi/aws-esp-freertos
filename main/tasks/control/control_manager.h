#ifndef CONTROL_MANAGER_H
#define CONTROL_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_err.h>

/**
 * @brief Initialize and start the control manager task.
 *
 * This function initializes necessary hardware and MQTT subscriptions,
 * then starts a FreeRTOS task to handle incoming MQTT messages for barrier control.
 */
void vStartControlManager(void);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_MANAGER_H */
