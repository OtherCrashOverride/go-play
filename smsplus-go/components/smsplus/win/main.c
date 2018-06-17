
#include "osd.h"
#include <allegro.h>
#include "shared.h"


BITMAP *bmp;
PALETTE pal;

void osd_init_input(void)
{
    install_keyboard();
#if 0
    if(install_joystick(JOY_TYPE_AUTODETECT) != 0)
    {
        printf("No joystick device present.");
    }
    else
    {
        int i, j;

        printf("Number of devices: %d\n", num_joysticks);

        for(i = 0; i < num_joysticks; i++)
        {
            printf("Device #%d\n", i);
            printf("Sticks: %d\n", joy[i].num_sticks);
            printf("Buttons: %d\n", joy[i].num_buttons);

            for(j = 0; j < joy[i].num_buttons; j++)
            {
                printf("\tButton #%d\n", j);
                printf("\tName: %s\n", joy[i].button[j].name);
            }

            for(j = 0; j < joy[i].num_sticks; j++)
            {
                printf("\tStick #%d\n", j);
                printf("\tAxis: %d (%s)\n", joy[i].stick[j].num_axis, joy[i].stick[j].flags & JOYFLAG_DIGITAL ? "digital" : "analog");
                printf("\tName: %s\n", joy[i].stick[j].name);
            }
        }
    }
#endif
}

void osd_update_input(void)
{
    /* Poll keyboard */
    if(keyboard_needs_poll())
        poll_keyboard();

    /* Poll joystick */
    poll_joystick();

    /* Reset all inputs */
    memset(&input, 0, sizeof(input));

    /* Keyboard: Check Player 1 inputs */
    if(key[KEY_LEFT])   input.pad[0] |= INPUT_LEFT;
    if(key[KEY_RIGHT])  input.pad[0] |= INPUT_RIGHT;
    if(key[KEY_UP])     input.pad[0] |= INPUT_UP;
    if(key[KEY_DOWN])   input.pad[0] |= INPUT_DOWN;
    if(key[KEY_A])      input.pad[0] |= INPUT_BUTTON2;
    if(key[KEY_S])      input.pad[0] |= INPUT_BUTTON1;
    if(key[KEY_ENTER])  input.system |= INPUT_START;

    /* Joystick: Check Player 1 inputs */
    if(joy[0].stick[0].axis[0].d1)   input.pad[0] |= INPUT_LEFT;
    if(joy[0].stick[0].axis[0].d2)   input.pad[0] |= INPUT_RIGHT;
    if(joy[0].stick[0].axis[1].d1)   input.pad[0] |= INPUT_UP;
    if(joy[0].stick[0].axis[1].d2)   input.pad[0] |= INPUT_DOWN;
    if(joy[0].button[0].b)  input.pad[0] |= INPUT_BUTTON2;
    if(joy[0].button[1].b)  input.pad[0] |= INPUT_BUTTON1;

    /* OSD controls */
    if(key[KEY_TAB])    system_reset();
}

void system_manage_sram(uint8 *sram, int slot, int mode)
{

}

int main (int argc, char *argv[])
{
    if(argc < 2)
    {
        printf("No filename specified.\n");
        exit(1);
    }

    strcpy(game_name, argv[1]);

    /* Attempt to load game off commandline */
    if(load_rom(game_name) == 0)
    {
        printf("Error loading `%s'.\n", game_name);
        exit(1);
    }

    allegro_init();
    install_timer();
    osd_init_input();
    set_color_depth(16);

    if(sms.console == CONSOLE_GG)
    {
        set_gfx_mode(GFX_AUTODETECT_WINDOWED, 160*4, 144*4, 0, 0);
    }
    else
    {
        set_gfx_mode(GFX_AUTODETECT_WINDOWED, 512, 384, 0, 0);
    }

    clear(screen);
    bmp = create_bitmap_ex(16, 256, 256);
    clear(bmp);

    /* Set up bitmap structure */
    memset(&bitmap, 0, sizeof(bitmap_t));
    bitmap.width  = bmp->w;
    bitmap.height = bmp->h;
    bitmap.depth  = 16;
    bitmap.granularity = 2; //
    bitmap.pitch  = bitmap.width * bitmap.granularity;
    bitmap.data   = (uint8 *)&bmp->line[0][0];
    bitmap.viewport.x = 0;
    bitmap.viewport.y = 0;
    bitmap.viewport.w = 256;
    bitmap.viewport.h = 192;

    snd.fm_which = SND_EMU2413;
    snd.fps = (1) ? FPS_NTSC : FPS_PAL;
    snd.fm_clock = (1) ? CLOCK_NTSC : CLOCK_PAL;
    snd.psg_clock = (1) ? CLOCK_NTSC : CLOCK_PAL;
    snd.sample_rate = 0;
    snd.mixer_callback = NULL;

    sms.territory = 0;
    sms.use_fm = 0;

    system_init();

    sms.territory = 0;
    sms.use_fm = 0;

    system_poweron();


    while(!key[KEY_ESC])
    {
        osd_update_input();
        system_frame(0);              
        vsync();
        stretch_blit(bmp, screen,
            bitmap.viewport.x,
            bitmap.viewport.y,
            bitmap.viewport.w,
            bitmap.viewport.h,
            0, 0, SCREEN_W, SCREEN_H);
    }

    system_poweroff();
    system_shutdown();

    return 0;
}
END_OF_MAIN();
