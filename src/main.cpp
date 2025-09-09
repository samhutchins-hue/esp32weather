#include "bmp280.h"
#include "driver/i2c_master.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace AppConfig {
constexpr gpio_num_t I2CMASTER_SCL_IO{GPIO_NUM_22};
constexpr gpio_num_t I2C_MASTER_SDA_IO{GPIO_NUM_21};
constexpr int I2C_MASTER_FREQ_HZ{400000};
constexpr const char *TAG = "BMP280";
constexpr char *WIFI_SSID = "hilltop";
constexpr char *WIFI_PASS = "YOUR_WIFI_PASSWORD";
constexpr const char *MQTT_TOPIC = "raspi/sensors/data";
} // namespace AppConfig

// static esp_err_t msqtt_client_start() {
//   const esp_mqtt_client_config_t mqtt_client_config = {
//       .broker = {.address = {.uri = "mqtt://YOUR_BROKER_IP"}}};
//   esp_mqtt_event_handle_t mqtt_event_handler;
//   esp_mqtt_client_handle_t client =
//   esp_mqtt_client_init(&mqtt_client_config); return
//   esp_mqtt_client_start(client);
// }

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGW(AppConfig::TAG, "WiFi disconnected, retrying...");
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ESP_LOGI(AppConfig::TAG, "Got IP, connected to WiFi");
  }
}

extern "C" void app_main(void) {

  // setup flash memory
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

  wifi_config_t wifi_config{};
  strncpy((char *)wifi_config.sta.ssid, AppConfig::WIFI_SSID,
          sizeof(wifi_config.sta.ssid));
  strncpy((char *)wifi_config.sta.password, AppConfig::WIFI_PASS,
          sizeof(wifi_config.sta.password));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  ESP_ERROR_CHECK(esp_wifi_start());

  // if (msqtt_client_start() != ESP_OK) {
  //   ESP_LOGE(AppConfig::TAG, "Failed to start MQTT client");
  // } else {
  //   ESP_LOGI(AppConfig::TAG, "MQTT client started successfully");
  // }

  i2c_master_bus_config_t i2c_bus_config = {
      .i2c_port = I2C_NUM_0,
      .sda_io_num = AppConfig::I2C_MASTER_SDA_IO,
      .scl_io_num = AppConfig::I2CMASTER_SCL_IO,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags = {.enable_internal_pullup = true}};

  i2c_master_bus_handle_t bus_handle;
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));
  ESP_LOGI(AppConfig::TAG, "Bus created successfully.");

  bmp280_config_t bmp280_cfg = I2C_BMP280_CONFIG_DEFAULT;
  bmp280_cfg.i2c_address = I2C_BMP280_DEV_ADDR_LO;

  bmp280_handle_t bmp280_dev_hdl = NULL; // Initialize to NULL

  ESP_ERROR_CHECK(bmp280_init(bus_handle, &bmp280_cfg, &bmp280_dev_hdl));

  if (bmp280_dev_hdl == NULL) {
    ESP_LOGE(AppConfig::TAG, "BMP280 initialization failed. Halting task.");
    while (1) {
      vTaskDelay(portMAX_DELAY);
    }
  }

  ESP_LOGI(AppConfig::TAG, "BMP280 initialized successfully!");

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(AppConfig::TAG, "###### BMP280 - START #######");

    float temperature, pressure;
    esp_err_t result =
        bmp280_get_measurements(bmp280_dev_hdl, &temperature, &pressure);
    if (result != ESP_OK) {
      ESP_LOGE(AppConfig::TAG, "BMP280 device read failed (%s)",
               esp_err_to_name(result));
    } else {
      // Pressure is in Pascals, convert to hectoPascals (hPa) / millibars (mb)
      pressure = pressure / 100;
      ESP_LOGI(AppConfig::TAG, "Temperature: %.2f C", temperature);
      ESP_LOGI(AppConfig::TAG, "Pressure: %.2f hPa", pressure);
    }

    ESP_LOGI(AppConfig::TAG, "######## BMP280 - END ########");
  }

  bmp280_delete(bmp280_dev_hdl);
  vTaskDelete(NULL);
}
