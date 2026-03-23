# esp32weather

A home weather station that captures temperature and pressure with an ESP32 from a BMP280 sensor and streams the data to an MQTT broker hosted by a Raspberry Pi, where the streamed data is stored in InfluxDB and visualized in Grafana.

## Architecture
  ```
  ESP32 (BMP280) --[JSON payload]--> Mosquitto <--[subscribe]-- Python subscriber
                                                                       \
                                                                      InfluxDB
                                                                          \
                                                                        Grafana
  ```
