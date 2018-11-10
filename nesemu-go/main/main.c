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
#include <string.h>

typedef uint8_t uint8;
#include "../components/nofrendo/nes/nesinput.h"

#include "../components/odroid/odroid_settings.h"
#include "../components/odroid/odroid_system.h"
#include "../components/odroid/odroid_sdcard.h"
#include "../components/odroid/odroid_display.h"
#include "../components/odroid/odroid_input.h"
#include "../components/odroid/odroid_audio.h"

const char *SD_BASE_PATH = "/sd";
static char *ROM_DATA; // = (char*)0x3f800000;

static bool forceConsoleReset;

typedef struct nes_s nes_t;

typedef struct rgb_s
{
    int r, g, b;
} rgb_t;

extern nes_t *nes_create(void);
extern int nes_insertcart(const char *filename, nes_t *machine);
extern void nes_start();
extern void nes_step();
extern uint8_t *nes_framebuffer_get();
extern rgb_t *nes_palette_get();

static nesinput_t nes_gamepad_0 = {INP_JOYPAD0, 0};
static uint16_t palette[256];

#define AUDIO_SAMPLE_RATE (44100)

char *osd_getromdata()
{
    printf("Initialized. ROM@%p\n", ROM_DATA);
    return (char *)ROM_DATA;
}

static void palette_copy()
{
    rgb_t *pal = nes_palette_get();
    for (int i = 0; i < 256; ++i)
    {
        uint16_t c = (pal[i].b >> 3) |
                     ((pal[i].g >> 2) << 5) |
                     ((pal[i].r >> 3) << 11);

        palette[i] = c; //(c >> 8) | (c << 8);
    }
}

#define AUDIO_SAMPLE_COUNT (44100 / 60)
static int16_t audio_buffer[AUDIO_SAMPLE_COUNT * 2];

extern int16_t audio_frame[];

void play_audio()
{
    for (int i = 0; i < AUDIO_SAMPLE_COUNT; ++i)
    {
        int16_t sample = audio_frame[i];
        audio_buffer[i * 2] = sample;
        audio_buffer[i * 2 + 1] = sample;
    }

    odroid_audio_submit((short *)audio_buffer, AUDIO_SAMPLE_COUNT);
}

static void display_frame()
{
    uint8_t *buffer = nes_framebuffer_get();
    ili9341_write_frame_nes(buffer, palette, /*uint8_t scale*/ 0);
}

volatile bool videoTaskIsRunning = false;
bool scaling_enabled = true;
bool previous_scaling_enabled = true;
QueueHandle_t vidQueue;
odroid_battery_state battery;

#define MX (256)
#define MY (224)
#define NES_STRIDE (MX + 16)

uint8_t frontBuffer[MX * MY];

static void videoTask(void *arg)
{
    uint8_t *param;

    videoTaskIsRunning = true;

    while (1)
    {
        xQueuePeek(vidQueue, &param, portMAX_DELAY);

        if (param == 1)
            break;

        if (previous_scaling_enabled != scaling_enabled)
        {
            ili9341_write_frame_nes(NULL, NULL, false);
            previous_scaling_enabled = scaling_enabled;
        }

        uint8_t *src = nes_framebuffer_get() + 8;
        uint8_t *dst = frontBuffer;

        for (int i = 0; i < MY; ++i)
        {
            memcpy(dst, src, MX);
            src += NES_STRIDE;
            dst += MX;
        }

        ili9341_write_frame_nes(frontBuffer, palette, scaling_enabled);

        odroid_input_battery_level_read(&battery);

        xQueueReceive(vidQueue, &param, portMAX_DELAY);
    }

    odroid_display_lock_sms_display();

    // Draw hourglass
    odroid_display_show_hourglass();

    odroid_display_unlock_sms_display();

    videoTaskIsRunning = false;
    vTaskDelete(NULL);

    while (1)
    {
    }
}

int app_main(void)
{
    printf("nesemu (%s-%s).\n", COMPILEDATE, GITREV);

    ROM_DATA = heap_caps_malloc(1 * 1024 * 1024, MALLOC_CAP_SPIRAM);
    if (!ROM_DATA)
        abort();

    nvs_flash_init();

    odroid_system_init();

    esp_err_t ret;

    int startHeap = esp_get_free_heap_size();
    printf("A HEAP:0x%x\n", startHeap);

    ili9341_init();

    // Joystick.
    odroid_input_gamepad_init();
    odroid_input_battery_level_init();

    //printf("osd_init: ili9341_prepare\n");
    ili9341_prepare();

    switch (esp_sleep_get_wakeup_cause())
    {
    case ESP_SLEEP_WAKEUP_EXT0:
    {
        printf("app_main: ESP_SLEEP_WAKEUP_EXT0 deep sleep reset\n");
        break;
    }

    case ESP_SLEEP_WAKEUP_EXT1:
    case ESP_SLEEP_WAKEUP_TIMER:
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
    case ESP_SLEEP_WAKEUP_ULP:
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    {
        printf("app_main: Unexpected deep sleep reset\n");
        odroid_gamepad_state bootState = odroid_input_read_raw();

        if (bootState.values[ODROID_INPUT_MENU])
        {
            // Force return to menu to recover from
            // ROM loading crashes

            // Set menu application
            odroid_system_application_set(0);

            // Reset
            esp_restart();
        }

        if (bootState.values[ODROID_INPUT_START])
        {
            // Reset emulator if button held at startup to
            // override save state
            forceConsoleReset = true; //emu_reset();
        }
    }
    break;

    default:
        printf("app_main: Not a deep sleep reset\n");
        break;
    }

    if (odroid_settings_StartAction_get() == ODROID_START_ACTION_RESTART)
    {
        forceConsoleReset = true;
        odroid_settings_StartAction_set(ODROID_START_ACTION_NORMAL);
    }

    // Load ROM
    char *romPath = odroid_settings_RomFilePath_get();
    if (!romPath)
    {
        // printf("osd_getromdata: Reading from flash.\n");

        // // copy from flash
        // spi_flash_mmap_handle_t hrom;

        // const esp_partition_t* part = esp_partition_find_first(0x40, 0, NULL);
        // if (part == 0)
        // {
        // 	printf("esp_partition_find_first failed.\n");
        // 	abort();
        // }

        // esp_err_t err = esp_partition_read(part, 0, (void*)ROM_DATA, 0x100000);
        // if (err != ESP_OK)
        // {
        // 	printf("esp_partition_read failed. size = %x (%d)\n", part->size, err);
        // 	abort();
        // }

        const char *ROM_FILE = "/sd/roms/nes/Super Mario Bros. (World).nes";
        romPath = malloc(strlen(ROM_FILE) + 1);
        memcpy(romPath, ROM_FILE, strlen(ROM_FILE) + 1);
    }
    //else
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

    odroid_audio_init(odroid_settings_AudioSink_get(), AUDIO_SAMPLE_RATE);

    vidQueue = xQueueCreate(1, sizeof(uint16_t *));
    xTaskCreatePinnedToCore(&videoTask, "videoTask", 1024 * 4, NULL, 5, NULL, 1);

    printf("NoFrendo start!\n");

    // ------

    // Clear display
    ili9341_write_frame_nes(NULL, NULL, 0);

    nes_t *nes = nes_create();

    input_register(&nes_gamepad_0);

    int nes_ret = nes_insertcart("(none)", nes);
    if (nes_ret != 0)
        abort();

    palette_copy();

    nes_start();

    odroid_gamepad_state previousState;
    odroid_input_gamepad_read(&previousState);

    uint32_t frameCount = 0;
    uint32_t skipFrame = 0;
    uint32_t startTime = 0;
    uint32_t stopTime = 0;
    uint32_t totalElapsedTime = 0;

    while (1)
    {
        startTime = xthal_get_ccount();

        // Input
        odroid_gamepad_state joystick;
        odroid_input_gamepad_read(&joystick);

        if (previousState.values[ODROID_INPUT_VOLUME] && !joystick.values[ODROID_INPUT_VOLUME])
        {
            odroid_audio_volume_change();
            printf("main: Volume=%d\n", odroid_audio_volume_get());
        }

        // Scaling
        if (joystick.values[ODROID_INPUT_START] && !previousState.values[ODROID_INPUT_RIGHT] && joystick.values[ODROID_INPUT_RIGHT])
        {
            scaling_enabled = !scaling_enabled;
            odroid_settings_ScaleDisabled_set(ODROID_SCALE_DISABLE_SMS, scaling_enabled ? 0 : 1);
        }

        nes_gamepad_0.data = 0;

        nes_gamepad_0.data |= joystick.values[ODROID_INPUT_A] ? INP_PAD_A : 0;
        nes_gamepad_0.data |= joystick.values[ODROID_INPUT_B] ? INP_PAD_B : 0;
        nes_gamepad_0.data |= joystick.values[ODROID_INPUT_SELECT] ? INP_PAD_SELECT : 0;
        nes_gamepad_0.data |= joystick.values[ODROID_INPUT_START] ? INP_PAD_START : 0;

        nes_gamepad_0.data |= joystick.values[ODROID_INPUT_UP] ? INP_PAD_UP : 0;
        nes_gamepad_0.data |= joystick.values[ODROID_INPUT_DOWN] ? INP_PAD_DOWN : 0;
        nes_gamepad_0.data |= joystick.values[ODROID_INPUT_LEFT] ? INP_PAD_LEFT : 0;
        nes_gamepad_0.data |= joystick.values[ODROID_INPUT_RIGHT] ? INP_PAD_RIGHT : 0;

        previousState = joystick;

        // Simulate
        nes_step();

        // Preset
        play_audio();

        if (!(skipFrame & 0x01))
        {
            uint8_t *temp = frontBuffer;
            xQueueSend(vidQueue, &temp, portMAX_DELAY);
        }

        // Stats
        stopTime = xthal_get_ccount();

        int elapsedTime;
        if (stopTime > startTime)
            elapsedTime = (stopTime - startTime);
        else
            elapsedTime = ((uint64_t)stopTime + (uint64_t)0xffffffff) - (startTime);

        totalElapsedTime += elapsedTime;

        ++frameCount;
        if (frameCount == 60)
        {
            float seconds = totalElapsedTime / (CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ * 1000000.0f);
            float fps = frameCount / seconds;

            printf("HEAP:0x%x (%#08x), FPS:%f, BATTERY:%d [%d]\n", esp_get_free_heap_size(), heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                   fps, battery.millivolts, battery.percentage);

            frameCount = 0;
            totalElapsedTime = 0;
        }

        if (skipFrame % 7 == 0)
        {
            ++skipFrame;
        }
        ++skipFrame;
    }
}
