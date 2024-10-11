# **AWS ESP IoT Project**

This project, **aws-esp-iot**, demonstrates how to use ESP32 for IoT applications with AWS IoT Core. It is based on ESP-IDF and showcases MQTT communication, device provisioning, sensor integration, and power management using ESP32.

**License**: This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Project Overview

This project is an end-to-end example that provides basic device provisioning, communication with AWS IoT, and sensor management using the ESP32 platform. The project demonstrates how to handle various functionalities like:

- Provisioning the device using Wi-Fi.
- Communicating with AWS IoT using MQTT.
- Reading sensor data (e.g., power sensors like INA3221).
- Detecting vehicle presence and obstacles.
- Control of barrier systems and buzzer alerts for parking management.

### Key Features

- **MQTT communication**: Publishes telemetry data and subscribes to command topics for remote control of the device.
- **Power telemetry**: Reads and sends power data from the INA3221 sensor.
- **Barrier control**: Detects obstacles and vehicles, controls parking barriers, and triggers alerts via a buzzer.
- **Wi-Fi telemetry**: Reports Wi-Fi signal strength (RSSI) and status.
- **Buzzer alerts**: Generates distinct sounds for different events like obstacle detection, vehicle detection, provisioning status, and more.

## How to Use This Project

### Getting Started

1. **Clone the project**:
   ```bash
   git clone https://github.com/your-github/aws-esp-iot.git
   cd aws-esp-iot
   ```

python components/espressif\_\_esp_secure_cert_mgr/tools/configure_esp_secure_cert.py -p /dev/cu.usbserial-576E0070601 --keep_ds_data_on_host --ca-cert main/certs/root_cert_auth.crt --device-cert main/certs/devicd.pem.crt --private-key main/certs/device-private.pem.key --target_chip esp32 --secure_cert_type cust_flash --priv_key_algo RSA, 2048
