#include "ina3221_sensor.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_MASTER_SCL_IO          22    // GPIO number for I2C master clock
#define I2C_MASTER_SDA_IO          21    // GPIO number for I2C master data
#define I2C_MASTER_NUM             I2C_NUM_1  // I2C port number for master
#define I2C_MASTER_FREQ_HZ         100000 // I2C master clock frequency
#define INA3221_ADDR               0x40   // Updated INA3221 I2C address
#define I2C_RETRY_COUNT            5      // Number of retry attempts for I2C operations

static const char *TAG = "INA3221";

void i2c_scanner(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus...");
    for (int i = 1; i < 127; i++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t espRc = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
        if (espRc == ESP_OK) {
            ESP_LOGI(TAG, "Found device at: 0x%02x", i);
        }
    }
    ESP_LOGI(TAG, "I2C scan completed.");
}

esp_err_t ina3221_init(void)
{
    // Configure I2C master
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &i2c_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(I2C_MASTER_NUM, i2c_conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    i2c_scanner(); // Call the I2C scanner to check for devices

    // Configure the INA3221 to enable all channels
    uint16_t config = INA3221_CONFIG_ENABLE_CH1 | INA3221_CONFIG_ENABLE_CH2 | INA3221_CONFIG_ENABLE_CH3 | INA3221_CONFIG_DEFAULT;
    uint8_t config_data[3] = { INA3221_REG_CONFIG, (config >> 8) & 0xFF, (config & 0xFF) };

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA3221_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, config_data, sizeof(config_data), true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure INA3221: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t ina3221_read(uint8_t reg, uint8_t *data_rd, size_t length)
{
    esp_err_t ret;
    i2c_cmd_handle_t cmd;

    for (int i = 0; i < I2C_RETRY_COUNT; i++) {
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (INA3221_ADDR << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, reg, true);
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (INA3221_ADDR << 1) | I2C_MASTER_READ, true);
        i2c_master_read(cmd, data_rd, length, I2C_MASTER_LAST_NACK);
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            return ESP_OK;
        }
        ESP_LOGW(TAG, "I2C read attempt %d failed: %s (reg: 0x%02x, len: %d)", i + 1, esp_err_to_name(ret), reg, length);
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }

    ESP_LOGE(TAG, "Failed to read from INA3221 after %d attempts: %s", I2C_RETRY_COUNT, esp_err_to_name(ret));
    return ret;
}

esp_err_t ina3221_read_channel(uint8_t channel, ina3221_reading_t *reading)
{
    uint8_t reg_shunt, reg_bus;
    switch (channel) {
        case 1:
            reg_shunt = INA3221_REG_SHUNT_VOLTAGE_CH1;
            reg_bus = INA3221_REG_BUS_VOLTAGE_CH1;
            break;
        case 2:
            reg_shunt = INA3221_REG_SHUNT_VOLTAGE_CH2;
            reg_bus = INA3221_REG_BUS_VOLTAGE_CH2;
            break;
        case 3:
            reg_shunt = INA3221_REG_SHUNT_VOLTAGE_CH3;
            reg_bus = INA3221_REG_BUS_VOLTAGE_CH3;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[2];

    // Read shunt voltage
    esp_err_t ret = ina3221_read(reg_shunt, data, 2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read shunt voltage from channel %d", channel);
        return ret;
    }
    int16_t shunt_voltage_raw = (data[0] << 8) | data[1];

    // Read bus voltage
    ret = ina3221_read(reg_bus, data, 2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read bus voltage from channel %d", channel);
        return ret;
    }
    int16_t bus_voltage_raw = (data[0] << 8) | data[1];

    // Calculate voltages and current
    reading->shunt_voltage = shunt_voltage_raw * 0.01; // Convert 10uV to mV
    reading->bus_voltage = bus_voltage_raw * 0.001; // Convert mV to V
    reading->load_voltage = reading->bus_voltage - (reading->shunt_voltage * 0.001); // Convert mV to V
    reading->current = reading->shunt_voltage / SHUNT_RESISTOR_OHMS; // Current in mA

    ESP_LOGI(TAG, "Channel %d - Bus Voltage: %.2f V, Shunt Voltage: %.2f mV, Load Voltage: %.2f V, Current: %.2f mA",
             channel, reading->bus_voltage, reading->shunt_voltage, reading->load_voltage, reading->current);

    return ESP_OK;
}
