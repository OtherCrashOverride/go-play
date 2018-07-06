#include "odroid_audio.h"

#include "odroid_settings.h"

#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "driver/i2s.h"
#include "driver/rtc_io.h"



#define I2S_NUM (I2S_NUM_0)
#define BUILTIN_DAC_ENABLED 1

static odroid_volume_level volumeLevel = ODROID_VOLUME_LEVEL4;
static int audio_sample_rate;

odroid_volume_level odroid_audio_volume_get()
{
    return volumeLevel;
}

void odroid_audio_volume_set(odroid_volume_level value)
{
    if (value >= ODROID_VOLUME_LEVEL_COUNT)
    {
        printf("odroid_audio_volume_set: value out of range (%d)\n", value);
        abort();
    }

    volumeLevel = value;
}

void odroid_audio_volume_change()
{
    int level = (volumeLevel + 1) % ODROID_VOLUME_LEVEL_COUNT;
    odroid_audio_volume_set(level);

    odroid_settings_Volume_set(level);
}

void odroid_audio_init(int sample_rate)
{
    audio_sample_rate = sample_rate;

    // NOTE: buffer needs to be adjusted per AUDIO_SAMPLE_RATE
# if BUILTIN_DAC_ENABLED

    i2s_config_t i2s_config = {
        //.mode = I2S_MODE_MASTER | I2S_MODE_TX,                                  // Only TX
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
        .sample_rate = audio_sample_rate,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           //2-channels
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        //.communication_format = I2S_COMM_FORMAT_PCM,
        .dma_buf_count = 4,
        //.dma_buf_len = 1472 / 2,  // (368samples * 2ch * 2(short)) = 1472
        .dma_buf_len = 512,  // (416samples * 2ch * 2(short)) = 1664
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,                                //Interrupt level 1
        .use_apll = 0 //1
    };

    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);

    i2s_set_pin(I2S_NUM, NULL);
    i2s_set_dac_mode(/*I2S_DAC_CHANNEL_LEFT_EN*/ I2S_DAC_CHANNEL_BOTH_EN);

#else

    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,                                  // Only TX
        .sample_rate = audio_sample_rate,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           //2-channels
        .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
        .dma_buf_count = 4,
        //.dma_buf_len = 1472 / 2,  // (368samples * 2ch * 2(short)) = 1472
        .dma_buf_len = 512,  // (416samples * 2ch * 2(short)) = 1664
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,                                //Interrupt level 1
        .use_apll = 1
    };

    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);


    i2s_pin_config_t pin_config = {
        .bck_io_num = 26,
        .ws_io_num = 25,
        .data_out_num = 27,
        .data_in_num = -1                                                       //Not used
    };
    i2s_set_pin(I2S_NUM, &pin_config);

#endif

    odroid_volume_level level = odroid_settings_Volume_get();
    odroid_audio_volume_set(level);
}

void odroid_audio_terminate()
{
    i2s_zero_dma_buffer(I2S_NUM);
    i2s_stop(I2S_NUM);

    i2s_start(I2S_NUM);


    esp_err_t err = rtc_gpio_init(GPIO_NUM_25);
    err = rtc_gpio_init(GPIO_NUM_26);
    if (err != ESP_OK)
    {
        abort();
    }

    err = rtc_gpio_set_direction(GPIO_NUM_25, RTC_GPIO_MODE_OUTPUT_ONLY);
    err = rtc_gpio_set_direction(GPIO_NUM_26, RTC_GPIO_MODE_OUTPUT_ONLY);
    if (err != ESP_OK)
    {
        abort();
    }

    err = rtc_gpio_set_level(GPIO_NUM_25, 0);
    err = rtc_gpio_set_level(GPIO_NUM_26, 0);
    if (err != ESP_OK)
    {
        abort();
    }
}

void odroid_audio_submit(short* stereoAudioBuffer, int frameCount)
{

    short currentAudioSampleCount = frameCount * 2;

#if BUILTIN_DAC_ENABLED
    // Convert for built in DAC
    for (short i = 0; i < currentAudioSampleCount; i += 2)
    {

        //convert stereo channels to mono by adding left and right channels to create a 17 bit value.
        int32_t sample = stereoAudioBuffer[i];
        sample += stereoAudioBuffer[i + 1];
//      sample += 0x8000;
        sample += 0x10000; //ensure 17bit integer is positive to avoid bitshifting negative signed integer.
        sample >>= volumeLevel; //Max volume is 1 as we need to convert back to 16bits.

        int16_t dac0 = volumeLevel ? (unsigned short)sample : 0x0000;

        const int16_t dac1 = volumeLevel ? 0x8000 : 0x0000;
        stereoAudioBuffer[i] = (int16_t)dac1;
        stereoAudioBuffer[i + 1] = dac0;

    }

#endif

    int len = currentAudioSampleCount * sizeof(int16_t);
    int count = i2s_write_bytes(I2S_NUM, (const char *)stereoAudioBuffer, len, portMAX_DELAY);
    if (count != len)
    {
        printf("i2s_write_bytes: count (%d) != len (%d)\n", count, len);
        abort();
    }

}

int odroid_audio_sample_rate_get()
{
    return audio_sample_rate;
}
