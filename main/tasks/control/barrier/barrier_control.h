#ifndef BARRIER_CONTROL_H
#define BARRIER_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_err.h>

/**
 * @brief Initialize and start the barrier control task.
 *
 * This function initializes necessary hardware and MQTT subscriptions,
 * then starts a FreeRTOS task to handle incoming MQTT messages for barrier control.
 */
void vStartBarrierControl(void);

#ifdef __cplusplus
}
#endif

#endif /* BARRIER_CONTROL_H */
