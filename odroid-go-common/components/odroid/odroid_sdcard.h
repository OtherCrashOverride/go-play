#pragma once

#include "esp_err.h"


esp_err_t odroid_sdcard_open();
esp_err_t odroid_sdcard_close();
size_t odroid_sdcard_copy_file_to_memory(const char* path, void* ptr);
