#include "bmp280.h"
#include "config.h"
#include "driver/i2c_master.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGW(WIFI, "WiFi disconnected, retrying...");
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ESP_LOGI(WIFI, "Got IP, connected to WiFi");
  }
}

static esp_err_t init_nvs(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(NVS, "NVS init failed, erasing and retrying");
    ret = nvs_flash_erase();
    if (ret != ESP_OK) {
      ESP_LOGE(NVS, "NVS erase failed: %s", esp_err_to_name(ret));
      return ret;
    }
    ret = nvs_flash_init();
    if (ret != ESP_OK) {
      ESP_LOGE(NVS, "NVS init failed: %s", esp_err_to_name(ret));
      return ret;
    }
  }
  ESP_LOGI(NVS, "NVS initialized successfully");
  return ESP_OK;
}

static esp_err_t init_wifi(void) {
  esp_err_t ret = esp_netif_init();
  if (ret != ESP_OK) {
    ESP_LOGE(WIFI, "Netif init failed: %s", esp_err_to_name(ret));
    return ret;
  }
  ret = esp_event_loop_create_default();
  if (ret != ESP_OK) {
    ESP_LOGE(WIFI, "Event loop creation failed: %s", esp_err_to_name(ret));
    return ret;
  }
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
  ret = esp_wifi_init(&wifi_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(WIFI, "WiFi init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                            &wifi_event_handler, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(WIFI, "WiFi event handler registration failed: %s",
             esp_err_to_name(ret));
    return ret;
  }
  ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                            &wifi_event_handler, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(WIFI, "IP event handler registration failed: %s",
             esp_err_to_name(ret));
    return ret;
  }

  wifi_config_t wifi_config = {.sta = {.ssid = {0}, .password = {0}}};
  strlcpy((char *)wifi_config.sta.ssid, WIFI_SSID,
          sizeof(wifi_config.sta.ssid));
  strlcpy((char *)wifi_config.sta.password, WIFI_PASS,
          sizeof(wifi_config.sta.password));

  ret = esp_wifi_set_mode(WIFI_MODE_STA);
  if (ret != ESP_OK) {
    ESP_LOGE(WIFI, "WiFi set mode failed: %s", esp_err_to_name(ret));
    return ret;
  }
  ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  if (ret != ESP_OK) {
    ESP_LOGE(WIFI, "WiFi set config failed: %s", esp_err_to_name(ret));
    return ret;
  }
  ret = esp_wifi_start();
  if (ret != ESP_OK) {
    ESP_LOGE(WIFI, "WiFi start failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(WIFI, "WiFi initialized successfully");
  return ESP_OK;
}

static esp_err_t init_i2c(i2c_master_bus_handle_t *bus_handle) {
  i2c_master_bus_config_t i2c_bus_config = {
      .i2c_port = I2C_NUM_0,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .scl_io_num = I2CMASTER_SCL_IO,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags = {.enable_internal_pullup = true}};

  esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, bus_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(I2C, "I2C bus init failed: %s", esp_err_to_name(ret));
    return ret;
  }
  ESP_LOGI(I2C, "I2C bus created successfully");
  return ESP_OK;
}

static esp_err_t init_bmp280(i2c_master_bus_handle_t bus_handle,
                             bmp280_handle_t *bmp280_dev_hdl) {

  bmp280_config_t bmp280_cfg = I2C_BMP280_CONFIG_DEFAULT;
  bmp280_cfg.i2c_address = I2C_BMP280_DEV_ADDR_LO;

  esp_err_t ret = bmp280_init(bus_handle, &bmp280_cfg, bmp280_dev_hdl);
  if (ret != ESP_OK || *bmp280_dev_hdl == NULL) {
    ESP_LOGE(BMP280, "BMP280 init failed: %s, handle: %p", esp_err_to_name(ret),
             *bmp280_dev_hdl);
    return ret != ESP_OK ? ret : ESP_FAIL;
  }
  ESP_LOGI(BMP280, "BMP280 initialized successfully, handle: %p",
           *bmp280_dev_hdl);
  return ESP_OK;
}

static void read_bmp280_task(void *pvParameters) {
  bmp280_handle_t bmp280_dev_hdl = (bmp280_handle_t)pvParameters;
  ESP_LOGI(BMP280, "BMP280 task started, handle: %p", bmp280_dev_hdl);

  if (bmp280_dev_hdl == NULL) {
    ESP_LOGE(BMP280, "Invalid BMP280 handle, stopping task");
    vTaskDelete(NULL);
    return;
  }

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(BMP280, "###### BMP280 - START #######");
    float temperature = 0;
    float pressure = 0;
    esp_err_t result =
        bmp280_get_measurements(bmp280_dev_hdl, &temperature, &pressure);
    if (result != ESP_OK) {
      ESP_LOGE(BMP280, "BMP280 read failed: %s", esp_err_to_name(result));
    } else {
      pressure /= 100; // Convert Pa to hPa
      ESP_LOGI(BMP280, "Temperature: %.2f C", temperature);
      ESP_LOGI(BMP280, "Pressure: %.2f hPa", pressure);
    }
    ESP_LOGI(BMP280, "######## BMP280 - END ########");
  }
}

static void cleanup(i2c_master_bus_handle_t bus_handle,
                    bmp280_handle_t bmp280_dev_hdl) {
  if (bmp280_dev_hdl) {
    bmp280_delete(bmp280_dev_hdl);
    ESP_LOGI(BMP280, "BMP280 device deleted");
  }
  if (bus_handle) {
    i2c_del_master_bus(bus_handle);
    ESP_LOGI(I2C, "I2C bus deleted");
  }
}

void app_main(void) {
  esp_log_level_set("*", ESP_LOG_VERBOSE); // Enable verbose logging

  // initialize NVS
  if (init_nvs() != ESP_OK) {
    ESP_LOGE(NVS, "Stopping application due to NVS failure");
    return;
  }

  // initialize WiFi
  if (init_wifi() != ESP_OK) {
    ESP_LOGE(WIFI, "Stopping application due to WiFi failure");
    return;
  }

  // initialize I2C bus
  i2c_master_bus_handle_t bus_handle = NULL;
  if (init_i2c(&bus_handle) != ESP_OK) {
    ESP_LOGE(I2C, "Stopping application due to I2C failure");
    return;
  }

  // initialize BMP280
  bmp280_handle_t bmp280_dev_hdl = NULL;
  if (init_bmp280(bus_handle, &bmp280_dev_hdl) != ESP_OK) {
    ESP_LOGE(BMP280, "Stopping application due to BMP280 failure");
    cleanup(bus_handle, NULL);
    return;
  }

  // start sensor reading task
  xTaskCreate(read_bmp280_task, "bmp280_task", 4096, bmp280_dev_hdl, 5, NULL);
}
