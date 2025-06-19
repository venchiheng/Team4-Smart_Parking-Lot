/*  
 * Project name: Smart parking lot
 * Author: Group 4 (Chiheng, Proseur, Sovan, Sakada)
 * Hardware: ESP32 (38 pins), Expansion board, RYG LED, TCRT500 x2, Servo Motor
 * Software framework: ESP-IDF v5.4.0 (VS Code Extension 1.9.1)
 * Project Description:
 * Simulates a smart parking system that auto-counts vehicle entries and exits,
 * opens/close the gate based on IR sensors, and updates cloud data.
 */

/*=============================================================================
 * HEADER FILE
 *===========================================================================*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"

/*=============================================================================
 * PROGRAM CONSTANT
 *===========================================================================*/
#define SERVO_GPIO GPIO_NUM_25
#define GREEN_LED GPIO_NUM_21
#define YELLOW_LED GPIO_NUM_22
#define RED_LED GPIO_NUM_23

#define MAX_PARKING 4   //Maximum Parking Space

#define WIFI_SSID "TGI-STUDENT"
#define WIFI_PASSWORD "tgi@@StuDent2024"
#define API_KEY "6UXB9AIE3A7PBJN1"

#define IR_SENSOR_ENTRY_CH ADC_CHANNEL_6 // GPIO34
#define IR_SENSOR_EXIT_CH ADC_CHANNEL_7  // GPIO35
#define SENSOR_TRIGGER_THRESHOLD 2000

/*=============================================================================
 * GLOBAL VARIABLES
 *===========================================================================*/
int vehicle_count = 0;
int daily_entry_count = 0;
adc_oneshot_unit_handle_t adc_handle;

/*=============================================================================
 * FUNCTION DECLARATION
 *===========================================================================*/
void wifi_init(void);
void adc_init(void);
void led_init(void);
void pwm_init(void);
void send_to_thingspeak(int count, int daily);
void set_led_state(bool green, bool yellow, bool red);
uint32_t angle_to_duty_cycle(uint8_t angle);
void Gate_Task(void);

/*=============================================================================
 * MAIN FUNCTION
 *===========================================================================*/
void app_main(void)
{
    printf("=================================================\nSmart Parking System Welcome :D\n=================================================\n");

    nvs_flash_init();   //Non-Volatile Storage
    wifi_init();
    adc_init();
    led_init();
    pwm_init();
    set_led_state(true, false, false);

    Gate_Task(); 
}

/*=============================================================================
 * FUNCTION DEFINITIONS
 *===========================================================================*/

// ADC configuration for IR sensors
void adc_init(void)
{   
    //Create channel
    adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&init_cfg, &adc_handle);

    //Configure channel
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_11
    };
    adc_oneshot_config_channel(adc_handle, IR_SENSOR_ENTRY_CH, &chan_cfg);
    adc_oneshot_config_channel(adc_handle, IR_SENSOR_EXIT_CH, &chan_cfg);
}

// PWM setup for servo motor control
void pwm_init(void)
{
    //Configure Timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,  // use Timer0 for LEDC
        .duty_resolution = LEDC_TIMER_12_BIT, // 12-bit resolution: 0-4096  
        .freq_hz = 50,  // use 50Hz frequency to avoid overheat 
        .clk_cfg = LEDC_AUTO_CLK    // automatically select based on resolution
    };
    ledc_timer_config(&ledc_timer);

    //Configure channel
    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,  // initial duty value
        .gpio_num = SERVO_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint = 0,    // start the pulse as soon as ref signal appear
        .timer_sel = LEDC_TIMER_0
    };
    ledc_channel_config(&ledc_channel);
}

// LED GPIO pin configuration
void led_init(void)
{
    //Configure GPIO
    gpio_config_t led_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GREEN_LED) | (1ULL << YELLOW_LED) | (1ULL << RED_LED),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);
}

// WiFi connection setup for ThingSpeak
void wifi_init(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}

// Convert angle (0-90) to PWM duty cycle for servo
uint32_t angle_to_duty_cycle(uint8_t angle)
{
    if (angle > 90) angle = 90;
    float pulse_width = 0.5 + (angle / 90.0) * 2.0;
    return (uint32_t)((pulse_width / 20.0) * 4096);
}

// Set the LED colors based on state
void set_led_state(bool green, bool yellow, bool red)
{
    gpio_set_level(GREEN_LED, green);
    gpio_set_level(YELLOW_LED, yellow);
    gpio_set_level(RED_LED, red);
}

// Send parking data to ThingSpeak
void send_to_thingspeak(int count, int daily)
{
    char url[256];
    snprintf(url, sizeof(url),
             "http://api.thingspeak.com/update?api_key=%s&field1=%d&field2=%d",
             API_KEY, count, daily);

    esp_http_client_config_t config = { .url = url };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

// Main loop task for managing entry/exit and servo
void Gate_Task(void)
{
    while (1)
    {
        //Threshold 2000
        // Read ADC values for both sensors
        int entry_adc = 0, exit_adc = 0;
        adc_oneshot_read(adc_handle, IR_SENSOR_ENTRY_CH, &entry_adc);
        adc_oneshot_read(adc_handle, IR_SENSOR_EXIT_CH, &exit_adc);

        printf("IR Sensor Readings - Entry: %d, Exit: %d\n", entry_adc, exit_adc);

        // ===============================
        // ENTRY DETECTION & ACTION
        // ===============================
        if (entry_adc < SENSOR_TRIGGER_THRESHOLD && vehicle_count < MAX_PARKING)
        {
            adc_oneshot_read(adc_handle, IR_SENSOR_ENTRY_CH, &entry_adc);
            vTaskDelay(50 / portTICK_PERIOD_MS);

            if (entry_adc < SENSOR_TRIGGER_THRESHOLD)
            {
                printf("Confirmed Entry.\n");
                set_led_state(false, true, false); // Yellow LED: gate opening
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, angle_to_duty_cycle(90));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
                vTaskDelay(3000 / portTICK_PERIOD_MS);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, angle_to_duty_cycle(0));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

                vehicle_count++;
                daily_entry_count++;
                printf("Vehicle Entered. Count: %d\n", vehicle_count);
                send_to_thingspeak(vehicle_count, daily_entry_count);   //Send updated data to cloud
            }
        }

        // ===============================
        // EXIT DETECTION & ACTION
        // ===============================
        else if (exit_adc < SENSOR_TRIGGER_THRESHOLD && vehicle_count > 0)
        {
            adc_oneshot_read(adc_handle, IR_SENSOR_EXIT_CH, &exit_adc);
            vTaskDelay(50 / portTICK_PERIOD_MS);

            if (exit_adc < SENSOR_TRIGGER_THRESHOLD)
            {
                printf("Confirmed Exit.\n");
                set_led_state(false, true, false); // Yellow LED: gate opening
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, angle_to_duty_cycle(90)); //Open Gate
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
                vTaskDelay(3000 / portTICK_PERIOD_MS);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, angle_to_duty_cycle(0)); //Close Gate
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

                vehicle_count--;
                printf("Vehicle Exited. Count: %d\n", vehicle_count);
                send_to_thingspeak(vehicle_count, daily_entry_count);
            }
        }

        // Update LED status
        set_led_state(vehicle_count < MAX_PARKING, false, vehicle_count >= MAX_PARKING);
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}
