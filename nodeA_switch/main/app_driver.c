/* Switch demo implementation using button and RGB LED
   
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <sdkconfig.h>

#include <iot_button.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h> 


#include <app_reset.h>
#include <ws2812_led.h>
#include "app_priv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ssd1306.h"
#include "font8x8_basic.h"

#define tag "Driver"



/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0

/* This is the GPIO on which the power will be set */
#define OUTPUT_GPIO    CONFIG_EXAMPLE_OUTPUT_GPIO
static bool g_power_state = DEFAULT_POWER;

/* These values correspoind to H,S,V = 120,100,10 */
#define DEFAULT_RED     25
#define DEFAULT_GREEN   0
#define DEFAULT_BLUE    0


#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    10

//General Declaration and Instantiation
SSD1306_t dev; //Add init for ssd1306
int center, top, bottom;
int pin_state1 = false; //NodeB Input
int pin_state2 = false; //NodeC Input
//image array declaration
uint8_t image[768] = {0};  // 6 pages × 128 columns
uint8_t image2[768] = {0};  // 6 pages × 128 columns
uint8_t image3[768] = {0};  // 6 pages × 128 columns

char lineChar[20];
char temp_str[16]; 


static void app_indicator_set(bool state)
{
    if (state) {
        ws2812_led_set_rgb(DEFAULT_RED, DEFAULT_GREEN, DEFAULT_BLUE);
    } else {
        ws2812_led_clear();
    }
}

static void app_indicator_init(void)
{
    ws2812_led_init();
    app_indicator_set(g_power_state);
}
static void push_btn_cb(void *arg)
{
    bool new_state = !g_power_state;
    app_driver_set_state(new_state);
#ifdef CONFIG_EXAMPLE_ENABLE_TEST_NOTIFICATIONS
    /* This snippet has been added just to demonstrate how the APIs esp_rmaker_param_update_and_notify()
     * and esp_rmaker_raise_alert() can be used to trigger push notifications on the phone apps.
     * Normally, there should not be a need to use these APIs for such simple operations. Please check
     * API documentation for details.
     */
    if (new_state) {
        esp_rmaker_param_update_and_notify(
                esp_rmaker_device_get_param_by_name(switch_device, ESP_RMAKER_DEF_POWER_NAME),
                esp_rmaker_bool(new_state));
    } else {
        esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_name(switch_device, ESP_RMAKER_DEF_POWER_NAME),
                esp_rmaker_bool(new_state));
        esp_rmaker_raise_alert("Switch was turned off");
    }
#else
    esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_name(switch_device, ESP_RMAKER_DEF_POWER_NAME),
            esp_rmaker_bool(new_state));
#endif
}

static void set_power_state(bool target)
{
    gpio_set_level(OUTPUT_GPIO, target); //pin 9 to output
    app_indicator_set(target);

}


// Bunny face bitmap (8-bit wide, 16 rows)
const uint8_t bunny2_16x16[16] = {
    0b00110011, // ....XX..XX
    0b00110011, // ....XX..XX
    0b00110011,
    0b00100001, // ....X....X
    0b00100001,
    0b01001110, // ...X..XX..X
    0b10000001,
    0b10011111, // ..X..XXXX..X
    0b10000001,
    0b10101010, // ..X.X.XX.X.X
    0b01000010,
    0b00101110, // ....X.XX.X
    0b00100010,
    0b00100010,
    0b00100010,
    0b00100010
};

const uint8_t sun1_32x16[16][4] = {
    {0b00000000, 0b00010000, 0b00001000, 0b00000000}, // .........X.......X..........
    {0b00000010, 0b00000000, 0b00000000, 0b10000000}, // ......X.............X.......
    {0b00000001, 0b00000111, 0b11100000, 0b10000000}, // .......X...XXXXXX....X......
    {0b00000000, 0b01111111, 0b11111110, 0b00000000}, // ........XXXXXXXXXXXXXX.......
    {0b00000000, 0b11111111, 0b11111111, 0b00000000}, // .......XXXXXXXXXXXXXXXX......
    {0b00000001, 0b11111111, 0b11111111, 0b10000000}, // ......XXXXXXXXXXXXXXXXXX.....
    {0b00010001, 0b11111111, 0b11111111, 0b10001000}, // ...X...XXXXXXXXXXXXXXXXX...X.
    {0b00000011, 0b11111111, 0b11111111, 0b11000000}, // ......XXXXXXXXXXXXXXXXXXXX....
    {0b00000011, 0b11111111, 0b11111111, 0b11000000}, // ......XXXXXXXXXXXXXXXXXXXX....
    {0b00010001, 0b11111111, 0b11111111, 0b10001000}, // ...X...XXXXXXXXXXXXXXXXX...X.
    {0b00000001, 0b11111111, 0b11111111, 0b10000000}, // ......XXXXXXXXXXXXXXXXXX.....
    {0b00000000, 0b11111111, 0b11111111, 0b00000000}, // .......XXXXXXXXXXXXXXXX......
    {0b00000000, 0b01111111, 0b11111110, 0b00000000}, // ........XXXXXXXXXXXXXX.......
    {0b00000001, 0b00000111, 0b11100000, 0b10000000}, // .......X...XXXXXX....X......
    {0b00000010, 0b00000000, 0b00000000, 0b10000000}, // ......X.............X.......
    {0b00000000, 0b00010000, 0b00001000, 0b00000000}  // .........X.......X..........
};
const uint8_t sun2_32x16[16][4] = {
    {0b00000000, 0b00010000, 0b00001000, 0b00000000}, // .........X.......X..........
    {0b00000010, 0b00000000, 0b00100000, 0b10000000}, // ......X.............X.......
    {0b00110001, 0b00000111, 0b11100000, 0b10000000}, // .......X...XXXXXX....X......
    {0b00000100, 0b01111111, 0b11111110, 0b00001000}, // ........XXXXXXXXXXXXXX.......
    {0b00000001, 0b11111111, 0b11111111, 0b00000100}, // .......XXXXXXXXXXXXXXXX......
    {0b00000001, 0b11111111, 0b11111111, 0b10000000}, // ......XXXXXXXXXXXXXXXXXX.....
    {0b00010001, 0b11111111, 0b11111111, 0b10001000}, // ...X...XXXXXXXXXXXXXXXXX...X.
    {0b00000011, 0b11111111, 0b11111111, 0b11000000}, // ......XXXXXXXXXXXXXXXXXXXX....
    {0b00000011, 0b11111111, 0b11111111, 0b11000000}, // ......XXXXXXXXXXXXXXXXXXXX....
    {0b00010001, 0b11111111, 0b11111111, 0b10001000}, // ...X...XXXXXXXXXXXXXXXXX...X.
    {0b00000101, 0b11111111, 0b11111111, 0b10000000}, // ......XXXXXXXXXXXXXXXXXX.....
    {0b00010000, 0b11111111, 0b11111111, 0b00000000}, // .......XXXXXXXXXXXXXXXX......
    {0b01000000, 0b01111111, 0b11111110, 0b00010000}, // ........XXXXXXXXXXXXXX.......
    {0b00000001, 0b00000111, 0b11100000, 0b10001000}, // .......X...XXXXXX....X......
    {0b00000010, 0b00000010, 0b00100000, 0b10001000}, // ......X.............X.......
    {0b00000000, 0b00010010, 0b00001000, 0b10000000}  // .........X.......X..........
};

// Define a function to draw a 32×16 sun bitmap
void draw_sun(uint8_t *image2, int offset_x, int offset_y) {
    // 32×16 sun bitmap (32 pixels wide, 16 pixels tall)
    // Each row needs 4 bytes (32 bits) to store the pattern

    
    // int offset_y = 0; // Start drawing from the top page
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 32; x++) {
            // Calculate which byte in the row contains this pixel
            int byte_idx = x / 8;
            // Calculate which bit in that byte represents this pixel
            int bit_position = 7 - (x % 8);
            
            // Check if this pixel is set
            if (sun1_32x16[y][byte_idx] & (1 << bit_position)) {
                for (int dx = 0; dx < 2; dx++) {  // Scale horizontally by 2
                    for (int dy = 0; dy < 2; dy++) {  // Scale vertically by 2
                        int px = offset_x + x * 2 + dx;
                        int py = offset_y * 8 + y * 2 + dy;
                        
                        int page = py / 8;
                        int bit = py % 8;
                        
                        if (page < 8 && px < 128 && px >= 0)  // Make sure it's within bounds
                            image2[page * 128 + px] |= (1 << bit);
                    }
                }
            }
        }
    }
}
void draw_sun2(uint8_t *image3, int offset_x, int offset_y) {
    // 32×16 sun bitmap (32 pixels wide, 16 pixels tall)
    // Each row needs 4 bytes (32 bits) to store the pattern

    
    // int offset_y = 0; // Start drawing from the top page
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 32; x++) {
            // Calculate which byte in the row contains this pixel
            int byte_idx = x / 8;
            // Calculate which bit in that byte represents this pixel
            int bit_position = 7 - (x % 8);
            
            // Check if this pixel is set
            if (sun2_32x16[y][byte_idx] & (1 << bit_position)) {
                for (int dx = 0; dx < 2; dx++) {  // Scale horizontally by 2
                    for (int dy = 0; dy < 2; dy++) {  // Scale vertically by 2
                        int px = offset_x + x * 2 + dx;
                        int py = offset_y * 8 + y * 2 + dy;
                        
                        int page = py / 8;
                        int bit = py % 8;
                        
                        if (page < 8 && px < 128 && px >= 0)  // Make sure it's within bounds
                            image3[page * 128 + px] |= (1 << bit);
                    }
                }
            }
        }
    }
}

void display_static_image2(int sun_x, int sun_y) {
    // Assuming image is already defined as a buffer of appropriate size
    uint8_t image2[8 * 128] = {0}; // 8 pages × 128 columns
    
    // Clear the buffer
    memset(image2, 0, sizeof(image2));
    
    // Draw the sun at the top of the screen
    // int sun_x = 16; // Position the sun on the left side
    draw_sun(image2, sun_x, sun_y);
    
    // Display the image2 once
    for (int page = 0; page < 8; page++) {
        ssd1306_display_image(&dev, page, 0, &image2[page * 128], 128);
    }
}

void display_static_image3(int sun_x, int sun_y) {
    // Assuming image is already defined as a buffer of appropriate size
    uint8_t image3[8 * 128] = {0}; // 8 pages × 128 columns
    
    // Clear the buffer
    memset(image3, 0, sizeof(image3));
    
    // Draw the sun at the top of the screen
    // int sun_x = 16; // Position the sun on the left side
    draw_sun2(image3, sun_x, sun_y);
    
    // Display the image3 once
    for (int page = 0; page < 8; page++) {
        ssd1306_display_image(&dev, page, 0, &image3[page * 128], 128);
    }
}

void display_animated_image3(int counter){

    for(int i=0; i<counter; i++){
        ssd1306_clear_screen(&dev, false);
        display_static_image2(32, 1);
        ssd1306_display_text(&dev, 6, "Rainmaker Proj!!", 16, false);
        ssd1306_display_text(&dev, 7, "  Start Program", 15, false);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        ssd1306_clear_screen(&dev, false);
        display_static_image3(32, 1);
        ssd1306_display_text(&dev, 6, "Rainmaker Proj!!", 16, false);
        ssd1306_display_text(&dev, 7, "  Start Program", 15, false);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

}

void draw_umbrella(uint8_t *image, int offset_x) {
    // 32×16 umbrella bitmap (32 pixels wide, 16 pixels tall)
    // Each row needs 4 bytes (32 bits) to store the pattern
    const uint8_t umbrella_32x16[16][4] = {
        {0b00000000, 0b00000111, 0b11100000, 0b00000000}, // ........XXXX....XXXX........
        {0b00000001, 0b11111111, 0b11111111, 0b10000000}, // .......XXXXXXXXXXXXXXXX......
        {0b00000011, 0b11111111, 0b11111111, 0b11100000}, // .....XXXXXXXXXXXXXXXXXXXXXXX....
        {0b00001111, 0b11111111, 0b11111111, 0b11110000}, // ....XXXXXXXXXXXXXXXXXXXXXXXX....
        {0b00011111, 0b11111111, 0b11111111, 0b11111000}, // ...XXXXXXXXXXXXXXXXXXXXXXXXXX...
        {0b00111111, 0b11111111, 0b11111111, 0b11111100}, // ..XXXXXXXXXXXXXXXXXXXXXXXXXXXX..
        {0b00011111, 0b11111111, 0b11111111, 0b11111000}, // ...XXXXXXXXXXXXXXXXXXXXXXXXXX...
        {0b00001111, 0b11111111, 0b11111111, 0b11110000}, // ....XXXXXXXXXXXXXXXXXXXXXXXX....
        {0b00000000, 0b00000011, 0b11000000, 0b00000000}, // ........XXXX....XXXX........
        {0b00000000, 0b00000011, 0b11000000, 0b00000000}, // ........XXXX....XXXX........
        {0b00000000, 0b00000011, 0b11000000, 0b00000000}, // ........XXXX....XXXX........
        {0b00000000, 0b00000011, 0b11000000, 0b00000000}, // ........XXXX....XXXX........
        {0b00000000, 0b00000011, 0b11001100, 0b00000000}, // ........XXXX....XXXX........
        {0b00000000, 0b00000011, 0b11001100, 0b00000000}, // .......XXXXXX..XXXXXX.......
        {0b00000000, 0b00000011, 0b11111100, 0b00000000}, // ......XXX..XXXX..XXX.......
        {0b00000001, 0b00000000, 0b11111000, 0b10000000}  // .....XXX....XX....XXX......
    };
    
    int offset_y = 1; // Start the umbrella from the middle vertical page (page 1)
    
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 32; x++) {
            // Calculate which byte in the row contains this pixel
            int byte_idx = x / 8;
            // Calculate which bit in that byte represents this pixel
            int bit_position = 7 - (x % 8);
            
            // Check if this pixel is set
            if (umbrella_32x16[y][byte_idx] & (1 << bit_position)) {
                for (int dx = 0; dx < 2; dx++) {  // Scale horizontally by 2
                    for (int dy = 0; dy < 2; dy++) {  // Scale vertically by 2
                        int px = offset_x + x * 2 + dx;
                        int py = offset_y * 8 + y * 2 + dy;
                        
                        int page = py / 8;
                        int bit = py % 8;
                        
                        if (page < 8 && px < 128 && px >= 0)  // Make sure it's within bounds
                            image[page * 128 + px] |= (1 << bit);
                    }
                }
            }
        }
    }
}

void print_moving_image(void){
    for (int offset = -64; offset <= 128; offset++) {  // Adjusted starting position for wider umbrella
        memset(image, 0, sizeof(image));  // Clear the screen buffer
        draw_umbrella(image, offset);     // Draw umbrella at the new offset
        
        for (int page = 0; page < 6; page++) {  // There are 8 pages for height (64 pixels)
            ssd1306_display_image(&dev, page, 0, &image[page * 128], 128);  // Display the image
        }
        
        // vTaskDelay(pdMS_TO_TICKS(80));  // Adjust delay for smooth animation speed
    }
}

void display_static_image() {
    // Assuming image is already defined as a buffer of appropriate size
    uint8_t image[8 * 128] = {0}; // 8 pages × 128 columns
    
    // Clear the buffer
    memset(image, 0, sizeof(image));
    
    // Draw the umbrella at a fixed position (e.g., centered)
    int center_x = (128 - (32 * 2)) / 2; // Center the 64-pixel wide scaled image
    draw_umbrella(image, center_x);
    
    // Display the image once
    for (int page = 0; page < 6; page++) {
        ssd1306_display_image(&dev, page, 0, &image[page * 128], 128);
    }
}

void send_alert_when_needed(void){
    // Create notification data

    esp_err_t err = esp_rmaker_raise_alert("Alert: FORGET TO TAKE ITEM!");
    // Send notification - no separate initialization needed
    if (err != ESP_OK) {
        ESP_LOGE(tag, "Failed to send alert: %d", err);
       
    } else {
        ESP_LOGI(tag, "Alert message sent successfully");
       
    }
}



void app_driver_init()
{

    #if CONFIG_I2C_INTERFACE
        ESP_LOGI(tag, "INTERFACE is i2c");
        ESP_LOGI(tag, "CONFIG_SDA_GPIO=%d",CONFIG_SDA_GPIO);
        ESP_LOGI(tag, "CONFIG_SCL_GPIO=%d",CONFIG_SCL_GPIO);
        ESP_LOGI(tag, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
        i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    #endif // CONFIG_I2C_INTERFACE

    #if CONFIG_SPI_INTERFACE
        ESP_LOGI(tag, "INTERFACE is SPI");
        ESP_LOGI(tag, "CONFIG_MOSI_GPIO=%d",CONFIG_MOSI_GPIO);
        ESP_LOGI(tag, "CONFIG_SCLK_GPIO=%d",CONFIG_SCLK_GPIO);
        ESP_LOGI(tag, "CONFIG_CS_GPIO=%d",CONFIG_CS_GPIO);
        ESP_LOGI(tag, "CONFIG_DC_GPIO=%d",CONFIG_DC_GPIO);
        ESP_LOGI(tag, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
        spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
    #endif // CONFIG_SPI_INTERFACE

    #if CONFIG_FLIP
        dev._flip = true;
        ESP_LOGW(tag, "Flip upside down");
    #endif

    #if CONFIG_SSD1306_128x64
        ESP_LOGI(tag, "Panel is 128x64");
        ssd1306_init(&dev, 128, 64);
    #endif // CONFIG_SSD1306_128x64
    #if CONFIG_SSD1306_128x32
        ESP_LOGI(tag, "Panel is 128x32");
        ssd1306_init(&dev, 128, 32);
    #endif // CONFIG_SSD1306_128x32

        ssd1306_clear_screen(&dev, false);
        ssd1306_contrast(&dev, 0xff);
        vTaskDelay(500 / portTICK_PERIOD_MS);


    #if CONFIG_SSD1306_128x64  //Show the starting screen upon turning on 
        top = 2;
        center = 3;
        bottom = 8;
 
        ssd1306_clear_screen(&dev, false);
        ssd1306_display_text(&dev, 2, "  Hello There!!", 15, false);
        ssd1306_display_text(&dev, 4, "  Initialising", 15, false);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        
        //Check GPIO State
        // ssd1306_display_text(&dev, 3, "Hello World!!", 13, false);
        // //ssd1306_clear_line(&dev, 4, true);
        // //ssd1306_clear_line(&dev, 5, true);
        // //ssd1306_clear_line(&dev, 6, true);
        // //ssd1306_clear_line(&dev, 7, true);
        // ssd1306_display_text(&dev, 4, "SSD1306 128x64", 14, true);
        // ssd1306_display_text(&dev, 5, "ABCDEFGHIJKLMNOP", 16, true);
        // ssd1306_display_text(&dev, 6, "abcdefghijklmnop",16, true);
        // ssd1306_display_text(&dev, 7, "Hello World!!", 13, true);
    #endif // CONFIG_SSD1306_128x64

    #if CONFIG_SSD1306_128x32
        top = 1;
        center = 1;
        bottom = 4;
        ssd1306_display_text(&dev, 0, "SSD1306 128x32", 14, false);
        ssd1306_display_text(&dev, 1, "Hello World!!", 13, false);
        //ssd1306_clear_line(&dev, 2, true);
        //ssd1306_clear_line(&dev, 3, true);
        ssd1306_display_text(&dev, 2, "SSD1306 128x32", 14, true);
        ssd1306_display_text(&dev, 3, "Hello World!!", 13, true);
    #endif // CONFIG_SSD1306_128x32
        vTaskDelay(3000 / portTICK_PERIOD_MS);

        button_handle_t btn_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
        if (btn_handle) {
            /* Register a callback for a button tap (short press) event */
            iot_button_set_evt_cb(btn_handle, BUTTON_CB_TAP, push_btn_cb, NULL);
            /* Register Wi-Fi reset and factory reset functionality on same button */
            app_reset_button_register(btn_handle, WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
        }

        /* Configure power */
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = 1,
        };
        io_conf.pin_bit_mask = ((uint64_t)1 << OUTPUT_GPIO); //set to pin 19
        /* Configure the GPIO */
        gpio_config(&io_conf);
        
        //Configure Input Pin1
        gpio_config_t input1 = {
            .mode = GPIO_MODE_INPUT,         // Set as input
            .pull_up_en = 0,                 // Enable pull-up resistor
            .pull_down_en = 0,               // Disable pull-down resistor
            .intr_type = GPIO_INTR_DISABLE,  // Disable interrupts (optional)
        };
        // Replace INPUT1 with your desired GPIO pin 1
        input1.pin_bit_mask = ((uint64_t)1 << 1);
        /* Configure the GPIO */
        gpio_config(&input1);

        //Configure Input Pin2
        gpio_config_t input2 = {
            .mode = GPIO_MODE_INPUT,         // Set as input
            .pull_up_en = 1,                 // Enable pull-up resistor
            .pull_down_en = 0,               // Disable pull-down resistor
            .intr_type = GPIO_INTR_DISABLE,  // Disable interrupts (optional)
        };
        // Replace INPUT2 with your desired GPIO pin 10
        input2.pin_bit_mask = ((uint64_t)1 << 10);
        /* Configure the GPIO */
        gpio_config(&input2);

        //Configure Input Pin2
        gpio_config_t output1 = {
            .mode = GPIO_MODE_OUTPUT,         // Set as input
            .pull_up_en = 1,                 // Enable pull-up resistor
            .pull_down_en = 0,               // Disable pull-down resistor
            .intr_type = GPIO_INTR_DISABLE,  // Disable interrupts (optional)
        };
        // Replace OUTPUT1 with your desired GPIO pin 4
        output1.pin_bit_mask = ((uint64_t)1 << 4);
        /* Configure the GPIO */
        gpio_config(&output1);
        gpio_set_level(4,1); //HIGH Buzzer: LOW,LOW Buzzer: HIGH

        app_indicator_init();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ws2812_led_set_rgb(25, 25, 25); //25 is max, all white
        esp_rmaker_param_update_and_report(power_param, esp_rmaker_bool(false));
        esp_rmaker_param_update_and_report(status_param,esp_rmaker_str("Starting..."));
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        display_animated_image3(5);
        

}

int IRAM_ATTR app_driver_set_state(bool state) //communicates with the mobile app switch power button
{
    /*
        - Set the app button to low
        - Read the gpio state updated from temp_update_task
        - Gets the gpio input at pin1
        - Set the app button to high
        - wait user pressed to confirm he taken the umbrella
        - when app button is low, say enjoy your day 
        - process repeats
    */ 
    //Check pin state, if pinstate1 is true, means its raining, switch power state to true
    // First check the umbrella sensor for rain prediction
    int rain_sensor_state = pin_state1;
        
    // If rain sensor indicates rain (HIGH), override the requested state
    // ORIGINAL CODE:
    if(g_power_state != state) {
        g_power_state = state;
        set_power_state(g_power_state); //turn on the LED, set pin 19 output HIGH
        if(g_power_state){
            send_alert_when_needed();
            ssd1306_clear_screen(&dev, false);
            ssd1306_display_text(&dev, 6, "  Its going to" ,19, false);
            ssd1306_display_text(&dev, 7, "      RAIIN" ,12, false);
            print_moving_image();
            display_static_image();
            ssd1306_display_text(&dev, 6, "Remember to take" ,17, false);
            ssd1306_display_text(&dev, 7, " your umbrella!" ,16, false);
            gpio_set_level(4,0); //pin4 output high: ON buzzer clarise side
            esp_rmaker_param_update_and_report(status_param,esp_rmaker_str("Expected Rain: Remember to bring your umbrella!!"));

        }else{
            ssd1306_clear_screen(&dev, false);
            display_static_image2(32,1);
            ssd1306_display_text(&dev, 6, " Have a pleasant",15, false);
            ssd1306_display_text(&dev, 7, "      day!!",11, false);
            esp_rmaker_param_update_and_report(status_param,esp_rmaker_str("Have a pleasant day!!"));
            gpio_set_level(4,1); //pin4 output high: OFF buzzer clarise side
        }
    }
    return ESP_OK;
}

bool app_driver_get_state(void)
{
    return g_power_state;
}


// The task function that updates the temperature parameter
void temp_update_task(void *pvParameters) {

    // if (temp_param == NULL) {
    //     ESP_LOGE("TEMP_TASK", "temp_param is NULL, cannot update temperature");
    //     vTaskDelete(NULL);
    //     return;
    // }
    // Task runs in an infinite loop
    int led_init_state = true; //turn white during init stages, turn off once after starting the loop
    while (1) {
        if(led_init_state){
            led_init_state = false;
            ws2812_led_clear();
            vTaskDelay(500 / portTICK_PERIOD_MS);
            ws2812_led_set_rgb(0, 25, 0); //set green, to tell user it is operationally ready
            for(int i = 0; i < 5; i++){
                ws2812_led_clear();
                vTaskDelay(250 / portTICK_PERIOD_MS);
                ws2812_led_set_rgb(0, 25, 0); //set green, to tell user it is operationally ready
                vTaskDelay(250 / portTICK_PERIOD_MS);
            }
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            ws2812_led_clear();
            esp_rmaker_param_update_and_report(power_param, esp_rmaker_bool(false));
            esp_rmaker_param_update_and_report(status_param,esp_rmaker_str("Start!!!"));
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }

        pin_state1 = gpio_get_level(1);//check incoming rain status from Michelle: HIGH: Expected Rain
        pin_state2 = gpio_get_level(10);//Check incoming rfid tag status from Clarise: HIGH: RFID tag scanned

        int current_btn_state = false;
        if(pin_state1 && pin_state2 && !g_power_state){//if pin state is high, and g_power_state is low
            esp_rmaker_param_update_and_report(power_param, esp_rmaker_bool(true)); //to switch on the mobile app, and mayb the appdriver_set_state()

            app_driver_set_state(true);//this will run and update the screen and led power indicator
            current_btn_state = true;
        }
        
        ESP_LOGE("TEMP_TASK_input_value:", "Input value: %d, %d", pin_state1, pin_state2);
        vTaskDelay(1000 / portTICK_PERIOD_MS);



        // Update and report the temperature parameter
        // new_temperature = rand() % (90 + 1);
        // // printf("%.2f", new_temp);
        // esp_rmaker_param_update_and_report(temp_param, esp_rmaker_float(new_temperature));
        // sprintf(temp_str, "%.2f", new_temperature);
        // ssd1306_display_text(&dev, 2, temp_str ,16, false);
        //pin_state always gets updated every second from gpio pin1
    }
    
    // The following line is never reached because the task runs in an infinite loop
    // But it's good practice to include it
    vTaskDelete(NULL);
}

// Function to create and start the update task
void start_temp_update_task(void) {
   // Create the task
   xTaskCreate(
       temp_update_task,     // Task function
       "temp_update_task",   // Name of the task (for debugging)
       4096,                 // Stack size (bytes)
       NULL,                 // Parameters passed to the task
       1,                    // Task priority (1 is low priority)
       NULL                  // Task handle
   );
}











// void draw_bunny(uint8_t *image, int offset_x) {
//     const uint8_t bunny_16x16[16] = {
//         0b00110011, // ....XX..XX
//         0b00110011,
//         0b00110011,
//         0b00100001, // ....X....X
//         0b00100001,
//         0b01011110, // ...X..XX..X
//         0b10000001,
//         0b10011111, // ..X..XXXX..X
//         0b10000001,
//         0b10101010, // ..X.X.XX.X.X
//         0b01000010,
//         0b00101110, // ....X.XX.X
//         0b00100010,
//         0b00100010,
//         0b00100010,
//         0b00100010
//     };

//     int offset_y = 1; // Start the bunny from the middle vertical page (page 1)

//     for (int y = 0; y < 16; y++) {
//         for (int x = 0; x < 8; x++) {
//             if (bunny_16x16[y] & (1 << (7 - x))) {
//                 for (int dx = 0; dx < 2; dx++) {  // Scale horizontally by 2
//                     for (int dy = 0; dy < 2; dy++) {  // Scale vertically by 2
//                         int px = offset_x + x * 2 + dx;
//                         int py = offset_y * 8 + y * 2 + dy;

//                         int page = py / 8;
//                         int bit = py % 8;

//                         if (page < 8 && px < 128 && px >= 0)  // Make sure it's within bounds
//                             image[page * 128 + px] |= (1 << bit);
//                     }
//                 }
//             }
//         }
//     }
// }

// // Draw 2x scale bunny
// int offset_x = 16; // horizontal center
// int offset_y = 1;  // page offset