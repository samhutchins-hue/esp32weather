#ifndef CONFIG_H
#define CONFIG_H

#define I2CMASTER_SCL_IO GPIO_NUM_22
#define I2C_MASTER_SDA_IO GPIO_NUM_21
#define I2C_MASTER_FREQ_HZ 400000
#define BMP280 "BMP280"
#define I2C "I2C"
#define WIFI "WIFI"
#define NVS "NVS"
#define WIFI_SSID "hilltop"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
#define MQTT_TOPIC "raspi/sensors/data"

#endif
