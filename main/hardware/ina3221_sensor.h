#ifndef INA3221_SENSOR_H
#define INA3221_SENSOR_H

#include "driver/i2c_master.h"
#include "esp_err.h"


// INA3221 Registers
#define INA3221_REG_CONFIG         0x00
#define INA3221_REG_SHUNT_VOLTAGE_CH1 0x01
#define INA3221_REG_BUS_VOLTAGE_CH1 0x02
#define INA3221_REG_SHUNT_VOLTAGE_CH2 0x03
#define INA3221_REG_BUS_VOLTAGE_CH2 0x04
#define INA3221_REG_SHUNT_VOLTAGE_CH3 0x05
#define INA3221_REG_BUS_VOLTAGE_CH3 0x06

// INA3221 Configuration Register Bits
#define INA3221_CONFIG_ENABLE_CH1  0x4000
#define INA3221_CONFIG_ENABLE_CH2  0x2000
#define INA3221_CONFIG_ENABLE_CH3  0x1000
#define INA3221_CONFIG_DEFAULT     0x7127

// Shunt resistor value in Ohms
#define SHUNT_RESISTOR_OHMS        0.1

// Struct to hold sensor readings
typedef struct {
    float bus_voltage;
    float shunt_voltage;
    float load_voltage;
    float current;
} ina3221_reading_t;

// Function prototypes
esp_err_t ina3221_init(void);
esp_err_t ina3221_read(uint8_t reg, uint8_t *data_rd, size_t length);
esp_err_t ina3221_read_channel(uint8_t channel, ina3221_reading_t *reading);

#endif // INA3221_SENSOR_H
