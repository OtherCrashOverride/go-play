/******************************************************************************
 *  Sega Master System / GameGear Emulator
 *  Copyright (C) 1998-2007  Charles MacDonald
 *
 *  additionnal code by Eke-Eke (SMS Plus GX)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *   Nintendo Gamecube State Management
 *
 ******************************************************************************/

#include "shared.h"

//static unsigned char* state = (unsigned char*)ESP32_PSRAM + 0x100000; //[0x10000];
//static unsigned int bufferptr;


int system_save_state(void *mem)
{
  int i;

  /* allocate temporary buffer */
  //bufferptr = 0;

  /*** Save VDP state ***/
  //memcpy (&state[bufferptr], &vdp, sizeof (vdp_t));
  //bufferptr += sizeof (vdp_t);
  fwrite(&vdp, sizeof(vdp), 1, mem);

  /*** Save SMS Context ***/
  //memcpy (&state[bufferptr], &sms, sizeof (sms_t));
  //bufferptr += sizeof (sms_t);
  fwrite(&sms, sizeof(sms), 1, mem);

  /*** Save cart info ***/
  for (i = 0; i < 4; i++)
  {
    //memcpy (&state[bufferptr], &cart.fcr[i], 1);
    //bufferptr++;
    fwrite(&cart.fcr[i], 1, 1, mem);
  }

  /*** Save SRAM ***/
  //memcpy (&state[bufferptr], cart.sram, 0x8000);
  //bufferptr += 0x8000;
  fwrite(&cart.sram[0], 0x8000, 1, mem);

  /*** Save Z80 Context ***/
  //memcpy (&state[bufferptr], &Z80, sizeof (Z80_Regs));
  //bufferptr += sizeof (Z80_Regs);
  fwrite(&Z80, sizeof(Z80), 1, mem);

#if 0
  /*** Save YM2413 ***/
  memcpy (&state[bufferptr], FM_GetContextPtr (), FM_GetContextSize ());
  bufferptr += FM_GetContextSize ();
#endif

  /*** Save SN76489 ***/
  //memcpy (&state[bufferptr], SN76489_GetContextPtr (0),SN76489_GetContextSize ());
  //bufferptr += SN76489_GetContextSize ();
  fwrite(SN76489_GetContextPtr(0), SN76489_GetContextSize(), 1, mem);

  /* write to FILE */
  //fwrite(&state[0], 0x10000, 1, mem);
  return 0;
}

void system_load_state(void *mem)
{
  int i;
  uint8 *buf;


  /* write to FILE */
  //fread(&state[0], 0x10000, 1, mem);

  /* Initialize everything */
  //bufferptr = 0;
  system_reset();

  /*** Set vdp state ***/
  //memcpy (&vdp, &state[bufferptr], sizeof (vdp_t));
  //bufferptr += sizeof (vdp_t);
  fread(&vdp, sizeof(vdp), 1, mem);

  /*** Set SMS Context ***/
  //memcpy (&sms, &state[bufferptr], sizeof (sms_t));
  //bufferptr += sizeof (sms_t);
  sms_t sms_tmp;
  fread(&sms_tmp, sizeof(sms), 1, mem);
  if(sms.console != sms_tmp.console)
  {
      system_reset();
      printf("%s: Bad save data\n", __func__);
       return;
   }
   sms = sms_tmp;

  /** restore video & audio settings (needed if timing changed) ***/
  vdp_init();
  sound_init();

  /*** Set cart info ***/
  for (i = 0; i < 4; i++)
  {
    //memcpy (&cart.fcr[i], &state[bufferptr], 1);
    //bufferptr++;
    fread(&cart.fcr[i], 1, 1, mem);
  }

  /*** Set SRAM ***/
  //memcpy (cart.sram, &state[bufferptr], 0x8000);
  //bufferptr += 0x8000;
  fread(&cart.sram[0], 0x8000, 1, mem);

  /*** Set Z80 Context ***/
  //memcpy (&Z80, &state[bufferptr], sizeof (Z80_Regs));
  //bufferptr += sizeof (Z80_Regs);
  fread(&Z80, sizeof(Z80), 1, mem);

#if 0
  /*** Set YM2413 ***/
  buf = malloc(FM_GetContextSize());
  memcpy (buf, &state[bufferptr], FM_GetContextSize ());
  FM_SetContext(buf);
  free(buf);
  bufferptr += FM_GetContextSize ();
#endif

  /*** Set SN76489 ***/
  //memcpy (SN76489_GetContextPtr(0), &state[bufferptr], SN76489_GetContextSize ());
  //bufferptr += SN76489_GetContextSize ();
  fread(SN76489_GetContextPtr(0), SN76489_GetContextSize(), 1, mem);

  if ((sms.console != CONSOLE_COLECO) && (sms.console != CONSOLE_SG1000))
  {
    /* Cartridge by default */
    slot.rom    = cart.rom;
    slot.pages  = cart.pages;
    slot.mapper = cart.mapper;
    slot.fcr = &cart.fcr[0];

    /* Restore mapping */
    mapper_reset();
    cpu_readmap[0]  = &slot.rom[0];
    if (slot.mapper != MAPPER_KOREA_MSX)
    {
      mapper_16k_w(0,slot.fcr[0]);
      mapper_16k_w(1,slot.fcr[1]);
      mapper_16k_w(2,slot.fcr[2]);
      mapper_16k_w(3,slot.fcr[3]);
    }
    else
    {
      mapper_8k_w(0,slot.fcr[0]);
      mapper_8k_w(1,slot.fcr[1]);
      mapper_8k_w(2,slot.fcr[2]);
      mapper_8k_w(3,slot.fcr[3]);
    }
  }

  /* Force full pattern cache update */
  bg_list_index = 0x200;
  for(i = 0; i < 0x200; i++)
  {
    bg_name_list[i] = i;
    bg_name_dirty[i] = -1;
  }

  /* Restore palette */
  for(i = 0; i < PALETTE_SIZE; i++)
    palette_sync(i);
}
