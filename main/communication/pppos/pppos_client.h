// pppos_client.h
#ifndef PPPOS_CLIENT_H
#define PPPOS_CLIENT_H

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"



/* Function prototypes */
esp_err_t pppos_client_init(void);
void pppos_client_start(void);
void pppos_client_stop(void);
void pppos_start(void);
#endif // PPPOS_CLIENT_H
