#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "nofrendo.h"
#include "esp_partition.h"
#include "esp_spiffs.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include <dirent.h>


#include "../components/odroid/odroid_settings.h"
#include "../components/odroid/odroid_system.h"


char *osd_getromdata()
{
	char* romdata;
	const esp_partition_t* part;
	spi_flash_mmap_handle_t hrom;
	esp_err_t err;


	int32_t dataSlot = odroid_settings_DataSlot_get();
	if (dataSlot < 0) dataSlot = 0;

	part=esp_partition_find_first(0x40, dataSlot, NULL);
	if (part==0)
	{
		printf("Couldn't find rom part!\n");
		abort();
	}

	err=esp_partition_mmap(part, 0, 1*1024*1024, SPI_FLASH_MMAP_DATA, (const void**)&romdata, &hrom);
	if (err!=ESP_OK)
	{
		printf("Couldn't map rom part!\n");
		abort();
	}

	printf("Initialized. ROM@%p\n", romdata);
    return (char*)romdata;
}




static const char *TAG = "main";

#define LCD_PIN_NUM_CS   CONFIG_HW_LCD_CS_GPIO

unsigned char buffer[512];

int app_main(void)
{
	printf("nesemu entered.\n");

	nvs_flash_init();

	odroid_system_init();

	esp_err_t ret;


	char* fileName;

	char* romName = odroid_settings_RomFilePath_get();
    if (romName)
    {
        fileName = odroid_util_GetFileName(romName);
        if (!fileName) abort();

		free(romName);
	}
	else
	{
		fileName = "nesemu-show3.nes";
	}


	int startHeap = esp_get_free_heap_size();
	printf("A HEAP:0x%x\n", startHeap);

	printf("Initializing SPIFFS\n");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/storage",
      .partition_label = NULL,
      .max_files = 1,
      .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            printf("Failed to mount or format filesystem.\n");
            abort();
        } else if (ret == ESP_ERR_NOT_FOUND) {
            printf("Failed to find SPIFFS partition.\n");
            abort();
        } else {
            printf("Failed to initialize SPIFFS (%d).\n", ret);
            abort();
        }
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        printf("Failed to get SPIFFS partition information. \n");
		abort();
    } else {
        printf("Partition size: total: %d, used: %d\n", total, used);
    }

	int stopHeap = esp_get_free_heap_size();
	printf("B HEAP:0x%x size=0x%x(%d)\n", stopHeap, startHeap - stopHeap, startHeap - stopHeap);


	printf("NoFrendo start!\n");

	char* args[1] = { fileName };
	nofrendo_main(1, args);

	printf("NoFrendo died.\n");
	asm("break.n 1");
    return 0;
}
