/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of version 2 of the GNU Library General 
** Public License as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** map185.c
**
** mapper 185 interface
*/

#include <string.h>
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>

static uint8 dummyvrom[0x400];
static uint8 chk;

static void map185_init(void)
{
    uint8* p = mmc_getinfo()->rom;
    if( p[0] == 0x78 && p[1] == 0xd8 && p[2] == 0xa2 && p[3] == 0x00 &&
        p[4] == 0x8e && p[5] == 0x00 && p[6] == 0x20 && p[7] == 0xad)
    { //Spy Vs Spy
        chk = 0x21;
    }
    else
    {
        chk = 0x03;
    }

    memset(dummyvrom, 0xff, sizeof(dummyvrom));
}

static void map185_write(uint32 address, uint8 value)
{
    UNUSED(address);

    if (((chk == 0x03) && (value & chk)) || ((chk != 0x03) && (value == chk)))
    {
        mmc_bankvrom(8, 0, 0);
    }
    else
    {
        ppu_setpage(8, 0, dummyvrom);
    }
}

static map_memwrite map185_memwrite[] =
{
    { 0x8000, 0xFFFF, map185_write },
    {     -1,     -1, NULL }
};

mapintf_t map185_intf = 
{
   185, /* mapper number */
   "CNROM(protect)", /* mapper name */
   map185_init, /* init routine */
   NULL, /* vblank callback */
   NULL, /* hblank callback */
   NULL, /* get state (snss) */
   NULL, /* set state (snss) */
   NULL, /* memory read structure */
   map185_memwrite, /* memory write structure */
   NULL /* external sound device */
};
