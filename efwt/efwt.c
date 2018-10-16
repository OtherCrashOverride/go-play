//               Copyright 2018 23pieces
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

char* FIRMWARE = "firmware.fw";
char* HEADER = "ODROIDGO_FIRMWARE_V00_01";

#define FIRMWARE_DESCRIPTION_SIZE (40)
char FirmwareDescription[FIRMWARE_DESCRIPTION_SIZE];

// ffmpeg -i tile.png -f rawvideo -pix_fmt rgb565 tile.raw
uint8_t tile[86 * 48 * 2];


int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("usage: %s firmware_file  output_file\n", argv[0]);
    }
    else
    {
        FILE* file = fopen(argv[1], "rb");
        if (!file)
        {
            printf("Couldn't open %s \n", argv[1]);
            printf("Exiting! \n");
            exit(1);
        }

        printf("Opened %s \n", argv[1]);

        FILE* output = fopen(argv[2], "wb");
        if (!output)
        {
            printf("Couldn't open %s \n", argv[2]);
            printf("Exiting! \n");
            exit(1);
        }
        printf("Opened %s \n", argv[2]);

        size_t count;

        char READ_HEADER[sizeof(HEADER)];

        count = fread(READ_HEADER, 1, sizeof(READ_HEADER), file);

        if(!strncmp(HEADER,READ_HEADER,strlen(HEADER)))
        {
            printf("HEADERS don't match! \n");
            printf("Exiting! \n");
            exit(2);
        }
        else
        {
            printf("HEADERS match\n");
            printf("HEADER='%s'\n", HEADER);
        }

        count = fread(FirmwareDescription, FIRMWARE_DESCRIPTION_SIZE, 1, file);
        printf("FirmwareDescription='%s'\n", FirmwareDescription);

        fseek ( file , 16 , SEEK_CUR); //mkfw writes 16 bytes offset after the description, set file postion 16 byte forward

        count = fread(tile, 1, sizeof(tile), file);
        if (count != sizeof(tile))
        {
            printf("Invalid tile\n");
            printf("Exiting! \n");
            exit(3);
        }

        count = fwrite(tile, 1, sizeof(tile), output);
        printf("tile: wrote %d bytes.\n", (int)count);

    }
    return 0;
}