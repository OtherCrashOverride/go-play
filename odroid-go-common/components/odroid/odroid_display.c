#include "odroid_display.h"

#include "image_splash.h"

#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/rtc_io.h"

#include <string.h>


const int DUTY_MAX = 0x1fff;

const gpio_num_t SPI_PIN_NUM_MISO = GPIO_NUM_19;
const gpio_num_t SPI_PIN_NUM_MOSI = GPIO_NUM_23;
const gpio_num_t SPI_PIN_NUM_CLK  = GPIO_NUM_18;

const gpio_num_t LCD_PIN_NUM_CS   = GPIO_NUM_5;
const gpio_num_t LCD_PIN_NUM_DC   = GPIO_NUM_21;
const gpio_num_t LCD_PIN_NUM_BCKL = GPIO_NUM_14;
const int LCD_BACKLIGHT_ON_VALUE = 1;
const int LCD_SPI_CLOCK_RATE = 40000000;


#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_ML  0x10
#define MADCTL_MH 0x04
#define TFT_RGB_BGR 0x08


static spi_transaction_t trans[8];
static spi_device_handle_t spi;
static spi_device_handle_t touch_spi;
static TaskHandle_t xTaskToNotify = NULL;
bool waitForTransactions = false;
bool isBackLightIntialized = false;

#define LINE_COUNT (5)
uint16_t* line[2];

#define GAMEBOY_WIDTH (160)
#define GAMEBOY_HEIGHT (144)

// SMS
#define GAME_WIDTH (256)
#define GAME_HEIGHT (192)

#define GAMEGEAR_WIDTH (160)
#define GAMEGEAR_HEIGHT (144)

#define PIXEL_MASK          (0x1F)

// NES
#define NES_GAME_WIDTH (256)
#define NES_GAME_HEIGHT (224) /* NES_VISIBLE_HEIGHT */

/*
 The ILI9341 needs a bunch of command/argument values to be initialized. They are stored in this struct.
*/
typedef struct {
    uint8_t cmd;
    uint8_t data[128];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} ili_init_cmd_t;

#define TFT_CMD_SWRESET	0x01
#define TFT_CMD_SLEEP 0x10
#define TFT_CMD_DISPLAY_OFF 0x28

DRAM_ATTR static const ili_init_cmd_t ili_sleep_cmds[] = {
    {TFT_CMD_SWRESET, {0}, 0x80},
    {TFT_CMD_DISPLAY_OFF, {0}, 0x80},
    {TFT_CMD_SLEEP, {0}, 0x80},
    {0, {0}, 0xff}
};


// 2.4" LCD
DRAM_ATTR static const ili_init_cmd_t ili_init_cmds[] = {
    // VCI=2.8V
    //************* Start Initial Sequence **********//
    {TFT_CMD_SWRESET, {0}, 0x80},
    {0xCF, {0x00, 0xc3, 0x30}, 3},
    {0xED, {0x64, 0x03, 0x12, 0x81}, 4},
    {0xE8, {0x85, 0x00, 0x78}, 3},
    {0xCB, {0x39, 0x2c, 0x00, 0x34, 0x02}, 5},
    {0xF7, {0x20}, 1},
    {0xEA, {0x00, 0x00}, 2},
    {0xC0, {0x1B}, 1},    //Power control   //VRH[5:0]
    {0xC1, {0x12}, 1},    //Power control   //SAP[2:0];BT[3:0]
    {0xC5, {0x32, 0x3C}, 2},    //VCM control
    {0xC7, {0x91}, 1},    //VCM control2
    //{0x36, {(MADCTL_MV | MADCTL_MX | TFT_RGB_BGR)}, 1},    // Memory Access Control
    {0x36, {(MADCTL_MV | MADCTL_MY | TFT_RGB_BGR)}, 1},    // Memory Access Control
    {0x3A, {0x55}, 1},
    {0xB1, {0x00, 0x1B}, 2},  // Frame Rate Control (1B=70, 1F=61, 10=119)
    {0xB6, {0x0A, 0xA2}, 2},    // Display Function Control
    {0xF6, {0x01, 0x30}, 2},
    {0xF2, {0x00}, 1},    // 3Gamma Function Disable
    {0x26, {0x01}, 1},     //Gamma curve selected

    //Set Gamma
    {0xE0, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00}, 15},
    {0XE1, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}, 15},

/*
    // LUT
    {0x2d, {0x01, 0x03, 0x05, 0x07, 0x09, 0x0b, 0x0d, 0x0f, 0x11, 0x13, 0x15, 0x17, 0x19, 0x1b, 0x1d, 0x1f,
            0x21, 0x23, 0x25, 0x27, 0x29, 0x2b, 0x2d, 0x2f, 0x31, 0x33, 0x35, 0x37, 0x39, 0x3b, 0x3d, 0x3f,
            0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
            0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c,
            0x1d, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x26, 0x27, 0x28, 0x29, 0x2a,
            0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
            0x00, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, 0x10, 0x12, 0x12, 0x14, 0x16, 0x18, 0x1a,
            0x1c, 0x1e, 0x20, 0x22, 0x24, 0x26, 0x26, 0x28, 0x2a, 0x2c, 0x2e, 0x30, 0x32, 0x34, 0x36, 0x38}, 128},
*/

    {0x11, {0}, 0x80},    //Exit Sleep
    {0x29, {0}, 0x80},    //Display on

    {0, {0}, 0xff}
};


//Send a command to the ILI9341. Uses spi_device_transmit, which waits until the transfer is complete.
static void ili_cmd(spi_device_handle_t spi, const uint8_t cmd)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    t.user=(void*)0;                //D/C needs to be set to 0
    ret=spi_device_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

//Send data to the ILI9341. Uses spi_device_transmit, which waits until the transfer is complete.
static void ili_data(spi_device_handle_t spi, const uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t;
    if (len==0) return;             //no need to send anything
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=len*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=data;               //Data
    t.user=(void*)1;                //D/C needs to be set to 1
    ret=spi_device_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.
static void ili_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc=(int)t->user;
    gpio_set_level(LCD_PIN_NUM_DC, dc);
}

static void ili_spi_post_transfer_callback(spi_transaction_t *t)
{
    if(xTaskToNotify && t == &trans[7])
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        /* Notify the task that the transmission is complete. */
        vTaskNotifyGiveFromISR(xTaskToNotify, &xHigherPriorityTaskWoken );

        if (xHigherPriorityTaskWoken)
            portYIELD_FROM_ISR();
    }
}


//Initialize the display
static void ili_init()
{
    int cmd=0;
    //Initialize non-SPI GPIOs
    gpio_set_direction(LCD_PIN_NUM_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_PIN_NUM_BCKL, GPIO_MODE_OUTPUT);


    //Send all the commands
    while (ili_init_cmds[cmd].databytes!=0xff) {
        ili_cmd(spi, ili_init_cmds[cmd].cmd);
        ili_data(spi, ili_init_cmds[cmd].data, ili_init_cmds[cmd].databytes&0x7F);
        if (ili_init_cmds[cmd].databytes&0x80) {
            vTaskDelay(100 / portTICK_RATE_MS);
        }
        cmd++;
    }
}


void send_reset_drawing(int left, int top, int width, int height)
{
  esp_err_t ret;

  trans[0].tx_data[0]=0x2A;           //Column Address Set
  trans[1].tx_data[0]=(left) >> 8;              //Start Col High
  trans[1].tx_data[1]=(left) & 0xff;              //Start Col Low
  trans[1].tx_data[2]=(left + width - 1) >> 8;       //End Col High
  trans[1].tx_data[3]=(left + width - 1) & 0xff;     //End Col Low
  trans[2].tx_data[0]=0x2B;           //Page address set
  trans[3].tx_data[0]=top >> 8;        //Start page high
  trans[3].tx_data[1]=top & 0xff;      //start page low
  trans[3].tx_data[2]=(top + height - 1)>>8;    //end page high
  trans[3].tx_data[3]=(top + height - 1)&0xff;  //end page low
  trans[4].tx_data[0]=0x2C;           //memory write

  //Queue all transactions.
  for (int x = 0; x < 5; x++) {
      ret=spi_device_queue_trans(spi, &trans[x], 1000 / portTICK_RATE_MS);
      assert(ret==ESP_OK);
  }

  // // Wait for all transactions
  // spi_transaction_t *rtrans;
  // for (int x = 0; x < 5; x++) {
  //     ret=spi_device_get_trans_result(spi, &rtrans, 1000 / portTICK_RATE_MS);
  //     assert(ret==ESP_OK);
  // }
}

void send_continue_wait()
{
  esp_err_t ret;

  if (waitForTransactions)
  {
    // Wait for all transactions
    // spi_transaction_t *rtrans;
    // for (int x = 0; x < 2; x++) {
    //     ret=spi_device_get_trans_result(spi, &rtrans, 1000 / portTICK_RATE_MS);
    //     assert(ret==ESP_OK);
    // }

    ulTaskNotifyTake(pdTRUE, 1000 / portTICK_RATE_MS /*portMAX_DELAY*/);

    // Drain SPI queue
    esp_err_t err = ESP_OK;
    while(err == ESP_OK)
    {
        spi_transaction_t* trans_desc;
        err = spi_device_get_trans_result(spi, &trans_desc, 0);

        //printf("ili9341_poweroff: removed pending transfer.\n");
    }

    waitForTransactions = false;
  }
}

void send_continue_line(uint16_t *line, int width, int lineCount)
{
  esp_err_t ret;

  // if (waitForTransactions)
  // {
  //   // Wait for all transactions
  //   spi_transaction_t *rtrans;
  //   for (int x = 0; x < 2; x++) {
  //       ret=spi_device_get_trans_result(spi, &rtrans, 1000 / portTICK_RATE_MS);
  //       assert(ret==ESP_OK);
  //   }
  //
  //   waitForTransactions = false;
  // }

  send_continue_wait();

  trans[6].tx_data[0] = 0x3C;           //memory write continue
  trans[6].length = 8;            //Data length, in bits
  trans[6].flags = SPI_TRANS_USE_TXDATA;

  trans[7].tx_buffer = line;            //finally send the line data
  trans[7].length = width * lineCount * 2 * 8;            //Data length, in bits
  trans[7].flags = 0; //undo SPI_TRANS_USE_TXDATA flag

  //Queue all transactions.
  for (int x = 6; x < 8; x++) {
      ret=spi_device_queue_trans(spi, &trans[x], 1000 / portTICK_RATE_MS);
      assert(ret==ESP_OK);
  }

#if 1
  waitForTransactions = true;
#else
  // Wait for all transactions
  spi_transaction_t *rtrans;
  for (int x = 0; x < 2; x++) {
      ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
      assert(ret==ESP_OK);
  }
#endif
}

static void backlight_init()
{
    // Note: In esp-idf v3.0, settings flash speed to 80Mhz causes the LCD controller
    // to malfunction after a soft-reset.

    // (duty range is 0 ~ ((2**bit_num)-1)


    //configure timer0
    ledc_timer_config_t ledc_timer;
    memset(&ledc_timer, 0, sizeof(ledc_timer));

    ledc_timer.bit_num = LEDC_TIMER_13_BIT; //set timer counter bit number
    ledc_timer.freq_hz = 5000;              //set frequency of pwm
    ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;   //timer mode,
    ledc_timer.timer_num = LEDC_TIMER_0;    //timer index


    ledc_timer_config(&ledc_timer);


    //set the configuration
    ledc_channel_config_t ledc_channel;
    memset(&ledc_channel, 0, sizeof(ledc_channel));

    //set LEDC channel 0
    ledc_channel.channel = LEDC_CHANNEL_0;
    //set the duty for initialization.(duty range is 0 ~ ((2**bit_num)-1)
    ledc_channel.duty = (LCD_BACKLIGHT_ON_VALUE) ? 0 : DUTY_MAX;
    //GPIO number
    ledc_channel.gpio_num = LCD_PIN_NUM_BCKL;
    //GPIO INTR TYPE, as an example, we enable fade_end interrupt here.
    ledc_channel.intr_type = LEDC_INTR_FADE_END;
    //set LEDC mode, from ledc_mode_t
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    //set LEDC timer source, if different channel use one timer,
    //the frequency and bit_num of these channels should be the same
    ledc_channel.timer_sel = LEDC_TIMER_0;


    ledc_channel_config(&ledc_channel);


    //initialize fade service.
    ledc_fade_func_install(0);

    // duty range is 0 ~ ((2**bit_num)-1)
    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (LCD_BACKLIGHT_ON_VALUE) ? DUTY_MAX : 0, 500);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);

    isBackLightIntialized = true;
}

#if 1
void backlight_percentage_set(int value)
{
    int duty = DUTY_MAX * (value * 0.01f);

    // //set the configuration
    // ledc_channel_config_t ledc_channel;
    // memset(&ledc_channel, 0, sizeof(ledc_channel));
    //
    // //set LEDC channel 0
    // ledc_channel.channel = LEDC_CHANNEL_0;
    // //set the duty for initialization.(duty range is 0 ~ ((2**bit_num)-1)
    // ledc_channel.duty = duty;
    // //GPIO number
    // ledc_channel.gpio_num = LCD_PIN_NUM_BCKL;
    // //GPIO INTR TYPE, as an example, we enable fade_end interrupt here.
    // ledc_channel.intr_type = LEDC_INTR_FADE_END;
    // //set LEDC mode, from ledc_mode_t
    // ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    // //set LEDC timer source, if different channel use one timer,
    // //the frequency and bit_num of these channels should be the same
    // ledc_channel.timer_sel = LEDC_TIMER_0;
    //
    //
    // ledc_channel_config(&ledc_channel);

    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty, 500);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
}
#endif

static uint16_t Blend(uint16_t a, uint16_t b)
{
  // Big endian
  // rrrrrGGG gggbbbbb

  char r0 = (a >> 11) & 0x1f;
  char g0 = (a >> 5) & 0x3f;
  char b0 = (a) & 0x1f;

  char r1 = (b >> 11) & 0x1f;
  char g1 = (b >> 5) & 0x3f;
  char b1 = (b) & 0x1f;

  uint16_t rv = ((r1 - r0) >> 1) + r0;
  uint16_t gv = ((g1 - g0) >> 1) + g0;
  uint16_t bv = ((b1 - b0) >> 1) + b0;

  return (rv << 11) | (gv << 5) | (bv);
}

void ili9341_write_frame_gb(uint16_t* buffer, int scale)
{
    short x, y;

    odroid_display_lock_gb_display();

    xTaskToNotify = xTaskGetCurrentTaskHandle();

    if (buffer == NULL)
    {
        // clear the buffer
        memset(line[0], 0, 320 * sizeof(uint16_t));

        // clear the screen
        send_reset_drawing(0, 0, 320, 240);

        for (y = 0; y < 240; ++y)
        {
        send_continue_line(line[0], 320, 1);
        }
    }
    else
    {
        uint16_t* framePtr = buffer;

        if (scale)
        {
            // NOTE: LINE_COUNT must be 3 or greater
            const short outputWidth = 265;
            const short outputHeight = 240;

            send_reset_drawing(26, 0, outputWidth, outputHeight);

            uint8_t alt = 0;
            for (y = 0; y < GAMEBOY_HEIGHT; y += 3)
            {
                for (int i = 0; i < 3; ++i)
                {
                    // skip middle vertical line
                    int index = i * outputWidth * 2;
                    int bufferIndex = ((y + i) * GAMEBOY_WIDTH);

                    for (x = 0; x < GAMEBOY_WIDTH; x += 3)
                    {
                        uint16_t a = framePtr[bufferIndex++];
                        uint16_t b;
                        uint16_t c;

                        if (x < GAMEBOY_WIDTH - 1)
                        {
                            b = framePtr[bufferIndex++];
                            c = framePtr[bufferIndex++];
                        }
                        else
                        {
                            b = framePtr[bufferIndex++];
                            c = 0;
                        }

                        uint16_t mid1 = Blend(a, b);
                        uint16_t mid2 = Blend(b, c);

                        line[alt][index++] = ((a >> 8) | ((a) << 8));
                        line[alt][index++] = ((mid1 >> 8) | ((mid1) << 8));
                        line[alt][index++] = ((b >> 8) | ((b) << 8));
                        line[alt][index++] = ((mid2 >> 8) | ((mid2) << 8));
                        line[alt][index++] = ((c >> 8) | ((c ) << 8));
                    }
                }

                // Blend top and bottom lines into middle
                short sourceA = 0;
                short sourceB = outputWidth * 2;
                short sourceC = sourceB + (outputWidth * 2);

                short output1 = outputWidth;
                short output2 = output1 + (outputWidth * 2);

                for (short j = 0; j < outputWidth; ++j)
                {
                    uint16_t a = line[alt][sourceA++];
                    a = ((a >> 8) | ((a) << 8));

                    uint16_t b = line[alt][sourceB++];
                    b = ((b >> 8) | ((b) << 8));

                    uint16_t c = line[alt][sourceC++];
                    c = ((c >> 8) | ((c) << 8));

                    uint16_t mid = Blend(a, b);
                    mid = ((mid >> 8) | ((mid) << 8));

                    line[alt][output1++] = mid;

                    uint16_t mid2 = Blend(b, c);
                    mid2 = ((mid2 >> 8) | ((mid2) << 8));

                    line[alt][output2++] = mid2;
                }

                // send the data
                send_continue_line(line[alt], outputWidth, 5);

                // swap buffers
                if (alt)
                    alt = 0;
                else
                    alt = 1;
            }
        }
        else
        {
            send_reset_drawing((320 / 2) - (GAMEBOY_WIDTH / 2),
                (240 / 2) - (GAMEBOY_HEIGHT / 2),
                GAMEBOY_WIDTH,
                GAMEBOY_HEIGHT);

            uint8_t alt = 0;

            for (y = 0; y < GAMEBOY_HEIGHT; y += LINE_COUNT)
            {
              int linesWritten = 0;

              for (int i = 0; i < LINE_COUNT; ++i)
              {
                  if((y + i) >= GAMEBOY_HEIGHT) break;

                  int index = (i) * GAMEBOY_WIDTH;
                  int bufferIndex = ((y + i) * GAMEBOY_WIDTH);

                  for (x = 0; x < GAMEBOY_WIDTH; ++x)
                  {
                    uint16_t sample = framePtr[bufferIndex++];
                    line[alt][index++] = ((sample >> 8) | ((sample & 0xff) << 8));
                  }

                  ++linesWritten;
              }

              send_continue_line(line[alt], GAMEBOY_WIDTH, linesWritten);

              // swap buffers
              if (alt)
                  alt = 0;
              else
                  alt = 1;
            }
        }
    }

    send_continue_wait();

    odroid_display_unlock_gb_display();

    //printf("FRAME: xs=%d, ys=%d, width=%d, height=%d, data=%p\n", xs, ys, width, height, data);
    //printf("sizeof(RGB565)=%d, sizeof(Pixel) =%d\n", sizeof(RGB565), sizeof(Pixel));
}

void ili9341_init()
{
    // Init
    const size_t lineSize = 320 * LINE_COUNT * sizeof(uint16_t);
    line[0] = heap_caps_malloc(lineSize, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!line[0]) abort();

    line[1] = heap_caps_malloc(lineSize, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!line[1]) abort();


	// Initialize transactions
    for (int x=0; x<8; x++) {
        memset(&trans[x], 0, sizeof(spi_transaction_t));
        if ((x&1)==0) {
            //Even transfers are commands
            trans[x].length=8;
            trans[x].user=(void*)0;
        } else {
            //Odd transfers are data
            trans[x].length=8*4;
            trans[x].user=(void*)1;
        }
        trans[x].flags=SPI_TRANS_USE_TXDATA;
    }

    // Initialize SPI
    esp_err_t ret;
    //spi_device_handle_t spi;
    spi_bus_config_t buscfg;
		memset(&buscfg, 0, sizeof(buscfg));

    buscfg.miso_io_num = SPI_PIN_NUM_MISO;
    buscfg.mosi_io_num = SPI_PIN_NUM_MOSI;
    buscfg.sclk_io_num = SPI_PIN_NUM_CLK;
    buscfg.quadwp_io_num=-1;
    buscfg.quadhd_io_num=-1;

    spi_device_interface_config_t devcfg;
		memset(&devcfg, 0, sizeof(devcfg));

    devcfg.clock_speed_hz = LCD_SPI_CLOCK_RATE;
    devcfg.mode = 0;                                //SPI mode 0
    devcfg.spics_io_num = LCD_PIN_NUM_CS;               //CS pin
    devcfg.queue_size = 7;                          //We want to be able to queue 7 transactions at a time
    devcfg.pre_cb = ili_spi_pre_transfer_callback;  //Specify pre-transfer callback to handle D/C line
    devcfg.post_cb = ili_spi_post_transfer_callback;
    devcfg.flags = 0; //SPI_DEVICE_HALFDUPLEX;

    //Initialize the SPI bus
    ret=spi_bus_initialize(VSPI_HOST, &buscfg, 1);
    assert(ret==ESP_OK);

    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(VSPI_HOST, &devcfg, &spi);
    assert(ret==ESP_OK);


    //Initialize the LCD
	printf("LCD: calling ili_init.\n");
    ili_init();

	printf("LCD: calling backlight_init.\n");
    backlight_init();

    printf("LCD Initialized (%d Hz).\n", LCD_SPI_CLOCK_RATE);
}

void ili9341_poweroff()
{
    // Drain SPI queue
    xTaskToNotify = 0;

    esp_err_t err = ESP_OK;

    while(err == ESP_OK)
    {
        spi_transaction_t* trans_desc;
        err = spi_device_get_trans_result(spi, &trans_desc, 0);

        printf("ili9341_poweroff: removed pending transfer.\n");
    }


    // fade off backlight
    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (LCD_BACKLIGHT_ON_VALUE) ? 0 : DUTY_MAX, 100);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_WAIT_DONE);


    // Disable LCD panel
    int cmd = 0;
    while (ili_sleep_cmds[cmd].databytes != 0xff)
    {
        //printf("ili9341_poweroff: cmd=%d, ili_sleep_cmds[cmd].cmd=0x%x, ili_sleep_cmds[cmd].databytes=0x%x\n",
        //    cmd, ili_sleep_cmds[cmd].cmd, ili_sleep_cmds[cmd].databytes);

        ili_cmd(spi, ili_sleep_cmds[cmd].cmd);
        ili_data(spi, ili_sleep_cmds[cmd].data, ili_sleep_cmds[cmd].databytes & 0x7f);
        if (ili_sleep_cmds[cmd].databytes & 0x80)
        {
            vTaskDelay(100 / portTICK_RATE_MS);
        }
        cmd++;
    }


    err = rtc_gpio_init(LCD_PIN_NUM_BCKL);
    if (err != ESP_OK)
    {
        abort();
    }

    err = rtc_gpio_set_direction(LCD_PIN_NUM_BCKL, RTC_GPIO_MODE_OUTPUT_ONLY);
    if (err != ESP_OK)
    {
        abort();
    }

    err = rtc_gpio_set_level(LCD_PIN_NUM_BCKL, LCD_BACKLIGHT_ON_VALUE ? 0 : 1);
    if (err != ESP_OK)
    {
        abort();
    }
}

void ili9341_prepare()
{
    // Return use of backlight pin
    esp_err_t err = rtc_gpio_deinit(LCD_PIN_NUM_BCKL);
    if (err != ESP_OK)
    {
        abort();
    }

#if 0
    // Disable backlight
    err = gpio_set_direction(LCD_PIN_NUM_BCKL, GPIO_MODE_OUTPUT);
    if (err != ESP_OK)
    {
        abort();
    }

    err = gpio_set_level(LCD_PIN_NUM_BCKL, LCD_BACKLIGHT_ON_VALUE ? 0 : 1);
    if (err != ESP_OK)
    {
        abort();
    }
#endif
}

//
void ili9341_write_frame_sms(uint8_t* buffer, uint8_t color[32][3], uint8_t isGameGear, uint8_t scale)
{
    short x, y;

    xTaskToNotify = xTaskGetCurrentTaskHandle();

    if (buffer == NULL)
    {
        // clear the buffer
        for (short i = 0; i < 2; ++i)
        {
            //memset(line[0], 0x00, 320 * sizeof(uint16_t));
            memset(line[i], 0x00, 320 * sizeof(uint16_t) * LINE_COUNT);
        }

        // clear the screen
        send_reset_drawing(0, 0, 320, 240);

        for (y = 0; y < 240; ++y)
        {
            send_continue_line(line[0], 320, 1);
        }

        send_continue_wait();
    }
    else
    {
        uint8_t* framePtr = buffer;


        if (!isGameGear)
        {
            if (scale)
            {
                const short xOffset = 4;
                const uint16_t displayWidth = 320 - (xOffset / 4 * 5);
                const short centerX = (320 - displayWidth) >> 1;

                send_reset_drawing(centerX, 0, displayWidth, 240);

                uint8_t alt = 0;

                for (y = 0; y < GAME_HEIGHT; y += 4)
                {
                  int linesWritten = 0;

                  for (short i = 0; i < 4; ++i)
                  {
                      if((y + i) >= GAME_HEIGHT)
                        break;

                      int index = (i) * displayWidth;
                      if (i > 1) index += displayWidth; // skip a line for blending

                      int bufferIndex = ((y + i) * GAME_WIDTH) + xOffset;

                      uint16_t samples[4];

                      for (x = 0; x < GAME_WIDTH - (xOffset * 2); x += 4)
                      {
                        for (short j = 0; j < 4; ++j)
                        {
                            uint8_t val = framePtr[bufferIndex++] & PIXEL_MASK;

                            uint8_t r = color[val][0];
                            uint8_t g = color[val][1];
                            uint8_t b = color[val][2];

                            samples[j] = (((r << 8) & 0xF800) | ((g << 3) & 0x07E0) | ((b >> 3) & 0x001F));
                        }

                        uint16_t mid1 = Blend(samples[1], samples[2]);

                        line[alt][index++] = ((samples[0] >> 8) | (samples[0] << 8));
                        line[alt][index++] = ((samples[1] >> 8) | (samples[1] << 8));
                        line[alt][index++] = ((mid1 >> 8) | (mid1 << 8));
                        line[alt][index++] = ((samples[2] >> 8) | (samples[2] << 8));
                        line[alt][index++] = ((samples[3] >> 8) | (samples[3] << 8));
                      }

                      ++linesWritten;
                  }

                  // blend horizontal
                  short srcIndex1 = displayWidth * 1;
                  short srcIndex2 = displayWidth * 3;
                  short dstIndex = displayWidth * 2;

                  for (short i = 0; i < displayWidth; ++i)
                  {
                      uint16_t sample1 = line[alt][srcIndex1++];
                      sample1 = ((sample1 >> 8) | (sample1 << 8));

                      uint16_t sample2 = line[alt][srcIndex2++];
                      sample2 = ((sample2 >> 8) | (sample2 << 8));

                      uint16_t mid1 = Blend(sample1, sample2);

                      line[alt][dstIndex++] = ((mid1 >> 8) | (mid1 << 8));
                  }

                  ++linesWritten;

                  // display
                  send_continue_line(line[alt], displayWidth, linesWritten);

                  // swap buffers
                  if (alt)
                    alt = 0;
                  else
                    alt = 1;
                }
            }
            else
            {
                send_reset_drawing((320 / 2) - (GAME_WIDTH / 2),
                    (240 / 2) - (GAME_HEIGHT / 2),
                    GAME_WIDTH,
                    GAME_HEIGHT);

                uint8_t alt = 0;

                for (y = 0; y < GAME_HEIGHT; y += 4)
                {
                  int linesWritten = 0;

                  for (short i = 0; i < 4; ++i)
                  {
                      if((y + i) >= GAME_HEIGHT)
                        break;

                      int index = (i) * GAME_WIDTH;
                      int bufferIndex = ((y + i) * GAME_WIDTH);

                      for (x = 0; x < GAME_WIDTH; ++x)
                      {
                        uint8_t val = framePtr[bufferIndex++] & PIXEL_MASK;

                        uint8_t r = color[val][0];
                        uint8_t g = color[val][1];
                        uint8_t b = color[val][2];

                        uint16_t sample = (((r << 8) & 0xF800) | ((g << 3) & 0x07E0) | ((b >> 3) & 0x001F));
                        line[alt][index++] = ((sample >> 8) | (sample << 8));
                      }

                      ++linesWritten;
                  }

                  // display
                  send_continue_line(line[alt], GAME_WIDTH, linesWritten);

                  // swap buffers
                  if (alt)
                    alt = 0;
                  else
                    alt = 1;
                }
            }
        }
        else
        {
            // game Gear
            framePtr += (24 * 256);

            if (scale)
            {
                const short outputWidth = 318;
                const short outputHeight = 240;
                const short centerX = (320 - outputWidth) >> 1;

                send_reset_drawing(centerX, 0, outputWidth, outputHeight);

                uint8_t alt = 0;
                for (y = 0; y < 144; y += 3)
                {
                    for (short i = 0; i < 3; ++i)
                    {
                        // skip middle vertical line
                        int index = i * outputWidth * 2;
                        int bufferIndex = ((y + i) * 256) + 48 + 1;

                        for (x = 1; x < GAMEGEAR_WIDTH - (1 * 2); ++x)
                        {
                            uint8_t val = framePtr[bufferIndex++] & PIXEL_MASK;

                            uint8_t r = color[val][0];
                            uint8_t g = color[val][1];
                            uint8_t b = color[val][2];

                            uint16_t sample = (((r << 8) & 0xF800) | ((g << 3) & 0x07E0) | ((b >> 3) & 0x001F));
                            sample = (sample >> 8) | (sample << 8);

                            line[alt][index++] = sample;
                            line[alt][index++] = sample;
                        }
                    }

                    // Blend top and bottom lines into middle
                    short sourceA = 0;
                    short sourceB = outputWidth * 2;
                    short sourceC = sourceB + (outputWidth * 2);

                    short output1 = outputWidth;
                    short output2 = output1 + (outputWidth * 2);

                    for (short j = 0; j < outputWidth; ++j)
                    {
                      uint16_t a = line[alt][sourceA++];
                      a = ((a >> 8) | ((a) << 8));

                      uint16_t b = line[alt][sourceB++];
                      b = ((b >> 8) | ((b) << 8));

                      uint16_t c = line[alt][sourceC++];
                      c = ((c >> 8) | ((c) << 8));

                      uint16_t mid = Blend(a, b);
                      mid = ((mid >> 8) | ((mid) << 8));

                      line[alt][output1++] = mid;

                      uint16_t mid2 = Blend(b, c);
                      mid2 = ((mid2 >> 8) | ((mid2) << 8));

                      line[alt][output2++] = mid2;
                    }

                    // send the data
                    send_continue_line(line[alt], outputWidth, 5);

                    // swap buffers
                    alt = alt ? 0 : 1;
                }
            }
            else
            {
                send_reset_drawing((320 / 2) - (GAMEGEAR_WIDTH / 2),
                    (240 / 2) - (GAMEGEAR_HEIGHT / 2),
                    GAMEGEAR_WIDTH,
                    GAMEGEAR_HEIGHT);

                uint8_t alt = 0;

                for (y = 0; y < GAMEGEAR_HEIGHT; y += LINE_COUNT)
                {
                  int linesWritten = 0;

                  for (short i = 0; i < LINE_COUNT; ++i)
                  {
                      if((y + i) >= GAMEGEAR_HEIGHT)
                        break;

                      int index = (i) * GAMEGEAR_WIDTH;
                      int bufferIndex = ((y + i) * 256) + 48;

                      for (x = 0; x < GAMEGEAR_WIDTH; ++x)
                      {
                        uint8_t val = framePtr[bufferIndex++] & PIXEL_MASK;

                        uint8_t r = color[val][0];
                        uint8_t g = color[val][1];
                        uint8_t b = color[val][2];

                        uint16_t sample = (((r << 8) & 0xF800) | ((g << 3) & 0x07E0) | ((b >> 3) & 0x001F));
                        line[alt][index++] = ((sample >> 8) | (sample << 8));
                      }

                      ++linesWritten;
                  }

                  // display
                  send_continue_line(line[alt], GAMEGEAR_WIDTH, linesWritten);

                  // swap buffers
                  if (alt)
                    alt = 0;
                  else
                    alt = 1;
                }
            }
        }

        send_continue_wait();
    }
}

//

void ili9341_write_frame_nes(uint8_t* buffer, uint16_t* myPalette, uint8_t scale)
{
    short x, y;

    xTaskToNotify = xTaskGetCurrentTaskHandle();

    if (buffer == NULL)
    {
        // clear the buffer
        memset(line[0], 0x00, 320 * sizeof(uint16_t));

        // clear the screen
        send_reset_drawing(0, 0, 320, 240);

        for (y = 0; y < 240; ++y)
        {
            send_continue_line(line[0], 320, 1);
        }
    }
    else
    {
        uint8_t* framePtr = buffer;

        if (scale)
        {
            const uint16_t displayWidth = 320 - 10;
            const uint16_t top = (240 - NES_GAME_HEIGHT) / 2;

            send_reset_drawing(0, top, displayWidth, NES_GAME_HEIGHT);

            uint8_t alt = 0;

            for (y = 0; y < NES_GAME_HEIGHT; y += LINE_COUNT)
            {
              int linesWritten = 0;

              for (short i = 0; i < LINE_COUNT; ++i)
              {
                  if((y + i) >= NES_GAME_HEIGHT)
                    break;

                  int index = (i) * displayWidth;

                  int bufferIndex = ((y + i) * NES_GAME_WIDTH) + 4;

                  uint16_t samples[4];
                  for (x = 4; x < NES_GAME_WIDTH - 4; x += 4)
                  {
                    for (short j = 0; j < 4; ++j)
                    {
                        uint8_t val = framePtr[bufferIndex++];
                        samples[j] = myPalette[val];
                    }

                    uint16_t mid = Blend(samples[1], samples[2]);

                    line[alt][index++] = samples[0];
                    line[alt][index++] = samples[1];
                    line[alt][index++] = mid;
                    line[alt][index++] = samples[2];
                    line[alt][index++] = samples[3];
                  }

                  ++linesWritten;
              }

              // display
              send_continue_line(line[alt], displayWidth, linesWritten);

              // swap buffers
              alt = alt ? 0 : 1;
            }
        }
        else
        {
            send_reset_drawing((320 / 2) - (NES_GAME_WIDTH / 2), (240 / 2) - (NES_GAME_HEIGHT / 2), NES_GAME_WIDTH, NES_GAME_HEIGHT);

            uint8_t alt = 0;

            for (y = 0; y < NES_GAME_HEIGHT; y += LINE_COUNT)
            {
              int linesWritten = 0;

              for (short i = 0; i < LINE_COUNT; ++i)
              {
                  if((y + i) >= NES_GAME_HEIGHT)
                    break;

                  int index = (i) * NES_GAME_WIDTH;
                  int bufferIndex = ((y + i) * NES_GAME_WIDTH);

                  for (x = 0; x < NES_GAME_WIDTH; ++x)
                  {
                    line[alt][index++] = myPalette[framePtr[bufferIndex++]];
                  }

                  ++linesWritten;
              }

              // display
              send_continue_line(line[alt], NES_GAME_WIDTH, linesWritten);

              // swap buffers
              alt = alt ? 0 : 1;
            }
        }
    }

    send_continue_wait();
}

void ili9341_write_frame(uint16_t* buffer)
{
    short x, y;

    xTaskToNotify = xTaskGetCurrentTaskHandle();

    if (buffer == NULL)
    {
        // clear the buffer
        memset(line[0], 0x00, 320 * sizeof(uint16_t));

        // clear the screen
        send_reset_drawing(0, 0, 320, 240);

        for (y = 0; y < 240; ++y)
        {
            send_continue_line(line[0], 320, 1);
        }

        send_continue_wait();
    }
    else
    {
        const int displayWidth = 320;
        const int displayHeight = 240;


        send_reset_drawing(0, 0, displayWidth, displayHeight);

        for (y = 0; y < displayHeight; y += 4)
        {
            send_continue_line(buffer + y * displayWidth, displayWidth, 4);
        }

        send_continue_wait();
    }
}

void ili9341_write_frame_rectangle(short left, short top, short width, short height, uint16_t* buffer)
{
    short x, y;

    if (left < 0 || top < 0) abort();
    if (width < 1 || height < 1) abort();

    xTaskToNotify = xTaskGetCurrentTaskHandle();

    send_reset_drawing(left, top, width, height);

    if (buffer == NULL)
    {
        // clear the buffer
        memset(line[0], 0x00, 320 * sizeof(uint16_t));

        // clear the screen
        for (y = 0; y < height; ++y)
        {
            send_continue_line(line[0], width, 1);
        }

        send_continue_wait();
    }
    else
    {
        short alt = 0;
        for (y = 0; y < height; y++)
        {
            memcpy(line[alt], buffer + y * width, width * sizeof(uint16_t));
            send_continue_line(line[alt], width, 1);

            ++alt;
            if (alt > 1) alt = 0;
        }

        send_continue_wait();
    }
}

void ili9341_clear(uint16_t color)
{
    xTaskToNotify = xTaskGetCurrentTaskHandle();

    send_reset_drawing(0, 0, 320, 240);

    // clear the buffer
    for (int i = 0; i < 320; ++i)
    {
        line[0][i] = color;
    }

    // clear the screen
    for (int y = 0; y < 240; ++y)
    {
        send_continue_line(line[0], 320, 1);
    }

    send_continue_wait();
}

void ili9341_write_frame_rectangleLE(short left, short top, short width, short height, uint16_t* buffer)
{
    short x, y;

    if (left < 0 || top < 0) abort();
    if (width < 1 || height < 1) abort();

    xTaskToNotify = xTaskGetCurrentTaskHandle();

    send_reset_drawing(left, top, width, height);

    if (buffer == NULL)
    {
        // clear the buffer
        memset(line[0], 0x00, 320 * sizeof(uint16_t));

        // clear the screen
        for (y = 0; y < height; ++y)
        {
            send_continue_line(line[0], width, 1);
        }

        send_continue_wait();
    }
    else
    {
        short alt = 0;
        for (y = 0; y < height; y++)
        {
            //memcpy(line[alt], buffer + y * width, width * sizeof(uint16_t));
            for (int i = 0; i < width; ++i)
            {
                uint16_t pixel = buffer[y * width + i];
                line[alt][i] = pixel << 8 | pixel >> 8;
            }

            send_continue_line(line[alt], width, 1);

            ++alt;
            if (alt > 1) alt = 0;
        }

        send_continue_wait();
    }
}

void display_tasktonotify_set(int value)
{
    xTaskToNotify = value;
}

int is_backlight_initialized()
{
    return isBackLightIntialized;
}

void odroid_display_show_splash()
{
    ili9341_write_frame_rectangleLE(0, 0, image_splash.width, image_splash.height, image_splash.pixel_data);

    // Drain SPI queue
    xTaskToNotify = 0;

    esp_err_t err = ESP_OK;

    while(err == ESP_OK)
    {
        spi_transaction_t* trans_desc;
        err = spi_device_get_trans_result(spi, &trans_desc, 0);

        //printf("odroid_display_show_splash: removed pending transfer.\n");
    }
}

void odroid_display_drain_spi()
{
    // Drain SPI queue
    xTaskToNotify = 0;

    esp_err_t err = ESP_OK;

    while(err == ESP_OK)
    {
        spi_transaction_t* trans_desc;
        err = spi_device_get_trans_result(spi, &trans_desc, 0);

        //printf("odroid_display_show_splash: removed pending transfer.\n");
    }
}

SemaphoreHandle_t gb_mutex = NULL;

void odroid_display_lock_gb_display()
{
    if (!gb_mutex)
    {
        gb_mutex = xSemaphoreCreateMutex();
        if (!gb_mutex) abort();
    }

    if (xSemaphoreTake(gb_mutex, 1000 / portTICK_RATE_MS) != pdTRUE)
    {
        abort();
    }
}

void odroid_display_unlock_gb_display()
{
    if (!gb_mutex) abort();

    xSemaphoreGive(gb_mutex);
}
