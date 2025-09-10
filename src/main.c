#include "bmp280.h"
#include "config.h"
#include "driver/i2c_master.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGW(TAG, "WiFi disconnected, retrying...");
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ESP_LOGI(TAG, "Got IP, connected to WiFi");
  }
}

// nvs initalization
static esp_err_t init_nvs(void) {

  // setup flash memory
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "NVS init failed, erasing and retrying");
    ret = nvs_flash_init();

    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "NVS erase failed: %s", esp_err_to_name(ret));
      return ret;
    }
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
    }

    return ret;
  }
}

// static esp_err_t msqtt_client_start() {
//   const esp_mqtt_client_config_t mqtt_client_config = {
//       .broker = {.address = {.uri = "mqtt://YOUR_BROKER_IP"}}};
//   esp_mqtt_event_handle_t mqtt_event_handler;
//   esp_mqtt_client_handle_t client =
//   esp_mqtt_client_init(&mqtt_client_config); return
//   esp_mqtt_client_start(client);
// }

// wifi initialization
static esp_err_t init_wifi(void) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

  wifi_config_t wifi_config = {.sta = {
                                   .ssid = {0},
                                   .password = {0},
                               }};

  strlcpy((char *)wifi_config.sta.ssid, WIFI_SSID,
          sizeof(wifi_config.sta.ssid));
  strlcpy((char *)wifi_config.sta.password, WIFI_PASS,
          sizeof(wifi_config.sta.password));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  return ESP_OK;
}

// i2c bus initalization
// bmp280 initialization
// sensor reading loop
// cleanup and task deletion

void app_main(void) {

  // Initialize NVS
  if (init_nvs() != ESP_OK) {
    ESP_LOGE(TAG, "Stopping application due to NVS failure");
    return;
  }

  if (init_wifi() != ESP_OK) {
    ESP_LOGE(TAG, "Stopping application due to WIFI failure");
    return;
  }

  // if (msqtt_client_start() != ESP_OK) {
  //   ESP_LOGE(AppConfig::TAG, "Failed to start MQTT client");
  // } else {
  //   ESP_LOGI(AppConfig::TAG, "MQTT client started successfully");
  // }

  i2c_master_bus_config_t i2c_bus_config = {
      .i2c_port = I2C_NUM_0,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .scl_io_num = I2CMASTER_SCL_IO,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags = {.enable_internal_pullup = true}};

  i2c_master_bus_handle_t bus_handle;
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));
  ESP_LOGI(TAG, "Bus created successfully.");

  bmp280_config_t bmp280_cfg = I2C_BMP280_CONFIG_DEFAULT;
  bmp280_cfg.i2c_address = I2C_BMP280_DEV_ADDR_LO;

  bmp280_handle_t bmp280_dev_hdl = NULL;
  ESP_ERROR_CHECK(bmp280_init(bus_handle, &bmp280_cfg, &bmp280_dev_hdl));
  if (bmp280_dev_hdl == NULL) {
    ESP_LOGE(TAG, "BMP280 initialization failed. Halting task.");
    while (1) {
      vTaskDelay(portMAX_DELAY);
    }
  }
  ESP_LOGI(TAG, "BMP280 initialized successfully!");

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "###### BMP280 - START #######");

    float temperature = 0;
    float pressure = 0;

    esp_err_t result =
        bmp280_get_measurements(bmp280_dev_hdl, &temperature, &pressure);
    if (result != ESP_OK) {
      ESP_LOGE(TAG, "BMP280 device read failed (%s)", esp_err_to_name(result));
    } else {
      pressure = pressure / 100;
      ESP_LOGI(TAG, "Temperature: %.2f C", temperature);
      ESP_LOGI(TAG, "Pressure: %.2f hPa", pressure);
    }

    ESP_LOGI(TAG, "######## BMP280 - END ########");
  }

  bmp280_delete(bmp280_dev_hdl);
  vTaskDelete(NULL);
}
