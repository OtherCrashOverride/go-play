#pragma once


#define ODROID_VOLUME_LEVEL_COUNT (5)

typedef enum
{
    ODROID_VOLUME_LEVEL0 = 0,
    ODROID_VOLUME_LEVEL1 = 4,
    ODROID_VOLUME_LEVEL2 = 3,
    ODROID_VOLUME_LEVEL3 = 2,
    ODROID_VOLUME_LEVEL4 = 1,

    _ODROID_VOLUME_FILLER = 0xffffffff
} odroid_volume_level;


odroid_volume_level odroid_audio_volume_get();
void odroid_audio_volume_set(odroid_volume_level value);
void odroid_audio_volume_change();
void odroid_audio_init(int sample_rate);
void odroid_audio_terminate();
void odroid_audio_submit(short* stereoAudioBuffer, int frameCount);
int odroid_audio_sample_rate_get();
