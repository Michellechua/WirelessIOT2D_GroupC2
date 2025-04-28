/* Switch Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>
#include <esp_rmaker_console.h>
#include <esp_rmaker_ota.h>

#include <esp_rmaker_common_events.h>

#include <app_network.h>
#include <app_insights.h>
#include "app_priv.h"

#include "esp32-dht11.h"  // Include DHT11 lib from abddellah2288

#define MFG_DEVICE_TYPE_SENSOR 1
#define MFG_DEVICE_SUBTYPE_DHT11 1

#define FACTORY_RESET_BUTTON_TIMEOUT 3
#define CONFIG_CONNECTION_TIMEOUT 5

#define DHT11_PIN GPIO_NUM_2 // Pin 2 to DHT11
#define RAIN_SENSOR_GPIO GPIO_NUM_4 // Pin 4 to Rain Sensor
#define ESP_SIGNAL GPIO_NUM_5 // Pin 5 to to Node A

static esp_rmaker_device_t *DHT11_device; // Rainmaker device for Weather Update
static esp_rmaker_param_t *temp_param; // Param for Temperature sensor (DHt11) on Rainmaker
static esp_rmaker_param_t *hum_param; // Param for Humidity sensor (DHT11) on Rainmaker
static esp_rmaker_param_t *rain_param; // Param for Rain sensor on Rainmaker

static const char *TAG = "app_main";

/* Event handler for catching RainMaker events */
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == RMAKER_EVENT) {
        switch (event_id) {
            case RMAKER_EVENT_INIT_DONE:
                ESP_LOGI(TAG, "RainMaker Initialised.");
                break;
            case RMAKER_EVENT_CLAIM_STARTED:
                ESP_LOGI(TAG, "RainMaker Claim Started.");
                break;
            case RMAKER_EVENT_CLAIM_SUCCESSFUL:
                ESP_LOGI(TAG, "RainMaker Claim Successful.");
                break;
            case RMAKER_EVENT_CLAIM_FAILED:
                ESP_LOGI(TAG, "RainMaker Claim Failed.");
                break;
            case RMAKER_EVENT_LOCAL_CTRL_STARTED:
                ESP_LOGI(TAG, "Local Control Started.");
                break;
            case RMAKER_EVENT_LOCAL_CTRL_STOPPED:
                ESP_LOGI(TAG, "Local Control Stopped.");
                break;
            default:
                ESP_LOGW(TAG, "Unhandled RainMaker Event: %"PRIi32, event_id);
        }
    } else if (event_base == RMAKER_COMMON_EVENT) {
        switch (event_id) {
            case RMAKER_EVENT_REBOOT:
                ESP_LOGI(TAG, "Rebooting in %d seconds.", *((uint8_t *)event_data));
                break;
            case RMAKER_EVENT_WIFI_RESET:
                ESP_LOGI(TAG, "Wi-Fi credentials reset.");
                break;
            case RMAKER_EVENT_FACTORY_RESET:
                ESP_LOGI(TAG, "Node reset to factory defaults.");
                break;
            case RMAKER_MQTT_EVENT_CONNECTED:
                ESP_LOGI(TAG, "MQTT Connected.");
                break;
            case RMAKER_MQTT_EVENT_DISCONNECTED:
                ESP_LOGI(TAG, "MQTT Disconnected.");
                break;
            case RMAKER_MQTT_EVENT_PUBLISHED:
                ESP_LOGI(TAG, "MQTT Published. Msg id: %d.", *((int *)event_data));
                break;
            default:
                ESP_LOGW(TAG, "Unhandled RainMaker Common Event: %"PRIi32, event_id);
        }
    } else if (event_base == APP_NETWORK_EVENT) {
        switch (event_id) {
            case APP_NETWORK_EVENT_QR_DISPLAY:
                ESP_LOGI(TAG, "Provisioning QR : %s", (char *)event_data);
                break;
            case APP_NETWORK_EVENT_PROV_TIMEOUT:
                ESP_LOGI(TAG, "Provisioning Timed Out. Please reboot.");
                break;
            case APP_NETWORK_EVENT_PROV_RESTART:
                ESP_LOGI(TAG, "Provisioning has restarted due to failures.");
                break;
            default:
                ESP_LOGW(TAG, "Unhandled App Wi-Fi Event: %"PRIi32, event_id);
                break;
        }
    } else if (event_base == RMAKER_OTA_EVENT) {
        switch(event_id) {
            case RMAKER_OTA_EVENT_STARTING:
                ESP_LOGI(TAG, "Starting OTA.");
                break;
            case RMAKER_OTA_EVENT_IN_PROGRESS:
                ESP_LOGI(TAG, "OTA is in progress.");
                break;
            case RMAKER_OTA_EVENT_SUCCESSFUL:
                ESP_LOGI(TAG, "OTA successful.");
                break;
            case RMAKER_OTA_EVENT_FAILED:
                ESP_LOGI(TAG, "OTA Failed.");
                break;
            case RMAKER_OTA_EVENT_REJECTED:
                ESP_LOGI(TAG, "OTA Rejected.");
                break;
            case RMAKER_OTA_EVENT_DELAYED:
                ESP_LOGI(TAG, "OTA Delayed.");
                break;
            case RMAKER_OTA_EVENT_REQ_FOR_REBOOT:
                ESP_LOGI(TAG, "Firmware image downloaded. Please reboot your device to apply the upgrade.");
                break;
            default:
                ESP_LOGW(TAG, "Unhandled OTA Event: %"PRIi32, event_id);
                break;
        }
    } else {
        ESP_LOGW(TAG, "Invalid event received!");
    }
}


/* Function for DHT11 Sensor */
void dht11_task(void *param)
{
    vTaskDelay(pdMS_TO_TICKS(3000)); // Dealy for system to stabilse

    dht11_t *dht11_sensor = (dht11_t *)param; // Link param to DHT11 Sensor 

    while (1) {
        if (!dht11_read(dht11_sensor, CONFIG_CONNECTION_TIMEOUT)){
            // Reading of DHT11 Sensor data
            float temperature = dht11_sensor->temperature;
            float humidity = dht11_sensor->humidity;

            // Print Temperature and Humidity readings to Terminal
            printf("Temperature: %.2f C\n", temperature);
            printf("Humudity: %.2f %%\n", humidity);

            // Update values to Rainmaker, under the device created
            esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_type(DHT11_device, ESP_RMAKER_PARAM_TEMPERATURE), 
                esp_rmaker_float(temperature));

            esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_type(DHT11_device, "esp.param.humidity"), 
                esp_rmaker_float(humidity));    

        }
 
        vTaskDelay(pdMS_TO_TICKS(2000)); // 2 secs delay before next reading
       
    }
}


/* Function for Rain Sensor */
void rain_task(void *param)
{
    while(1){
    bool is_raining = gpio_get_level(RAIN_SENSOR_GPIO) == 0;  // Checking rain sensor status (LOW = rain detected)

    const char *rain_text = is_raining ?  "It is raining! 🌧️" : "Not raining ☀️"; // True = raining : False = Not raining
    esp_rmaker_param_update_and_report(rain_param, esp_rmaker_str(rain_text)); // Update to rainmaker
    
    gpio_set_level(ESP_SIGNAL, is_raining ? 1 : 0); // Update to GPIO4 for Node A; Output 1 = Raining, Output 0 = No rain

    // For terminal
    ESP_LOGI("RainSensor", "Rain status: %s", rain_text);
    ESP_LOGI("RainSensor", "GPIO Level (pin %d): %d", RAIN_SENSOR_GPIO, gpio_get_level(RAIN_SENSOR_GPIO));

    vTaskDelay(pdMS_TO_TICKS(2000)); // 2 secs delay before next reading
    }

}



void app_main()
{
    /* Initialize Application specific hardware drivers and
     * set initial state.
     */
    
    esp_rmaker_console_init();
    app_driver_init();
    app_driver_set_state(DEFAULT_POWER);

    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    /* Initialize Wi-Fi. Note that, this should be called before esp_rmaker_node_init()
     */
    app_network_init();

    /* Register an event handler to catch RainMaker events */
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_COMMON_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(APP_NETWORK_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    /* Initialize the ESP RainMaker Agent.
     * Note that this should be called after app_network_init() but before app_nenetworkk_start()
     * */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };

    // Initialise Rainmaker
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Weather Update");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    // Initiaise DHT11 Sensor, assign GPIO
    static dht11_t dht11_sensor;
    dht11_sensor.dht11_pin = DHT11_PIN;


    // Create Weather Update device
    DHT11_device = esp_rmaker_device_create("Weather Update", "esp.device.temperature-sensor", NULL);

    if (!DHT11_device) {
        ESP_LOGE(TAG, "Could not create DHT11 device. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }


    // Create temperature param for DHT11 sensor
    temp_param = esp_rmaker_param_create(
        ESP_RMAKER_DEF_TEMPERATURE_NAME, // Param name
        ESP_RMAKER_PARAM_TEMPERATURE, // Param type
        esp_rmaker_float(0), // Default value
        PROP_FLAG_READ | PROP_FLAG_PERSIST
    );
    esp_rmaker_device_add_param(DHT11_device, temp_param); // Add temp param to device


    // Create humidity param for DHT11 sensor
    hum_param = esp_rmaker_param_create(
        "Humidity", // Param name
        "esp.param.humidity", // Param type
        esp_rmaker_float(0), // Default value
        PROP_FLAG_READ | PROP_FLAG_PERSIST
    );
    esp_rmaker_param_add_ui_type(hum_param, "esp.ui.text"); // Display as text
    esp_rmaker_device_add_param(DHT11_device, hum_param); // Add humidity param to device


    // For rain sensor
    gpio_reset_pin(RAIN_SENSOR_GPIO);
    gpio_set_direction(RAIN_SENSOR_GPIO, GPIO_MODE_INPUT); // Configure rain sensor GPIO to input

    // Create param for rain sensor 
    rain_param = esp_rmaker_param_create(
        "Raining", // Param name
        "esp.param.rain", // Param type
        esp_rmaker_str("Updating"), // Default value
        PROP_FLAG_READ | PROP_FLAG_PERSIST
    );
    esp_rmaker_param_add_ui_type(rain_param, "esp.ui.text"); // Display as text
    esp_rmaker_device_add_param(DHT11_device, rain_param); // Add rain param to device
    

    // Configuration for GPIO Output connection to Node A
    gpio_reset_pin(ESP_SIGNAL);
    gpio_set_direction(ESP_SIGNAL, GPIO_MODE_OUTPUT); // Output pin

    // Attach Weather Update device to Rainmaker node
    esp_rmaker_node_add_device(node, DHT11_device);


    /* Enable OTA */
    esp_rmaker_ota_enable_default();

    /* Enable timezone service which will be require for setting appropriate timezone
     * from the phone apps for scheduling to work correctly.
     * For more information on the various ways of setting timezone, please check
     * https://rainmaker.espressif.com/docs/time-service.html.
     */
    esp_rmaker_timezone_service_enable();

    /* Enable scheduling. */
    esp_rmaker_schedule_enable();

    /* Enable Scenes */
    esp_rmaker_scenes_enable();

    /* Enable Insights. Requires CONFIG_ESP_INSIGHTS_ENABLED=y */
    app_insights_enable();

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

    err = app_network_set_custom_mfg_data(MFG_DEVICE_TYPE_SENSOR, MFG_DEVICE_SUBTYPE_DHT11);

    
    // err = app_network_set_custom_mfg_data(MGF_DATA_DEVICE_TYPE_SWITCH, MFG_DATA_DEVICE_SUBTYPE_SWITCH);
    /* Start the Wi-Fi.
     * If the node is provisioned, it will start connection attempts,
     * else, it will start Wi-Fi provisioning. The function will return
     * after a connection has been successfully established
     */
    err = app_network_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }


    // Create task to read DHT11 sensor
    xTaskCreate(dht11_task, "dht11_task", 4096, &dht11_sensor, 5, NULL);

    // Create task to read rain sensor
    xTaskCreate(rain_task, "rain_task", 2048, NULL, 5, NULL);



}
