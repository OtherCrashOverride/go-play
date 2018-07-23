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
#include "../components/odroid/odroid_sdcard.h"
#include "../components/odroid/odroid_display.h"

const char* SD_BASE_PATH = "/sd";

static char* ROM_DATA = (char*)0x3f800000;;

char *osd_getromdata()
{
	printf("Initialized. ROM@%p\n", ROM_DATA);
	return (char*)ROM_DATA;
}




static const char *TAG = "main";


int app_main(void)
{
	printf("nesemu (%s-%s).\n", COMPILEDATE, GITREV);

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


	ili9341_init();


	// Load ROM
	char* romPath = odroid_settings_RomFilePath_get();
	if (!romPath)
	{
		printf("osd_getromdata: Reading from flash.\n");

		// copy from flash
		spi_flash_mmap_handle_t hrom;

		const esp_partition_t* part = esp_partition_find_first(0x40, 0, NULL);
		if (part == 0)
		{
			printf("esp_partition_find_first failed.\n");
			abort();
		}

		esp_err_t err = esp_partition_read(part, 0, (void*)ROM_DATA, 0x100000);
		if (err != ESP_OK)
		{
			printf("esp_partition_read failed. size = %x (%d)\n", part->size, err);
			abort();
		}
	}
	else
	{
		printf("osd_getromdata: Reading from sdcard.\n");

		// copy from SD card
		esp_err_t r = odroid_sdcard_open(SD_BASE_PATH);
		if (r != ESP_OK)
        {
            odroid_display_show_sderr(ODROID_SD_ERR_NOCARD);
            abort();
        }

		size_t fileSize = odroid_sdcard_copy_file_to_memory(romPath, ROM_DATA);
		printf("app_main: fileSize=%d\n", fileSize);
		if (fileSize == 0)
        {
            odroid_display_show_sderr(ODROID_SD_ERR_BADFILE);
            abort();
        }

		r = odroid_sdcard_close();
		if (r != ESP_OK)
        {
            odroid_display_show_sderr(ODROID_SD_ERR_NOCARD);
            abort();
        }

		free(romPath);
	}


	printf("NoFrendo start!\n");

	char* args[1] = { fileName };
	nofrendo_main(1, args);

	printf("NoFrendo died.\n");
	asm("break.n 1");
    return 0;
}
