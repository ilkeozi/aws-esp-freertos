#ifndef BARRIER_CONTROL_H
#define BARRIER_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
/**
 * @brief Initialize the barrier driver.
 */
void barrier_driver_init(void);

/**
 * @brief Open the barrier.
 */
void barrier_driver_open(void);

/**
 * @brief Close the barrier.
 */
void barrier_driver_close(void);

/**
 * @brief Test the limit switches and log their status.
 */
void barrier_driver_test_limit_switches(void);


/**
 * @brief Check if the barrier is locked.
 * 
 * @return true if the barrier is locked, false otherwise.
 */
bool barrier_driver_is_locked(void);

/**
 * @brief Check if the barrier is unlocked.
 * 
 * @return true if the barrier is unlocked, false otherwise.
 */
bool barrier_driver_is_unlocked(void);

#ifdef __cplusplus
}
#endif

#endif // BARRIER_CONTROL_H
