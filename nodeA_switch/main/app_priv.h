/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "stdio.h"

#define DEFAULT_POWER  false
extern esp_rmaker_device_t *switch_device;
extern esp_rmaker_param_t *status_param;
extern esp_rmaker_param_t *power_param;

void temp_update_task(void *pvParameters);
void start_temp_update_task(void);

void app_driver_init(void);
int app_driver_set_state(bool state);
bool app_driver_get_state(void);


// extern esp_rmaker_device_t *sensor_device;extern float new_temperature;
