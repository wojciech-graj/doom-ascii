//
// Copyright(C) 2022 Wojciech Graj
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     terminal-specific code
//

#include "doomgeneric.h"
#include "m_argv.h"
#include "i_video.h"

unsigned DOOMGENERIC_RESX = 80;
unsigned DOOMGENERIC_RESY = 50;

uint32_t* DG_ScreenBuffer = 0;


void dg_Create()
{
	int i;
	i = M_CheckParmWithArgs("-scaling", 1);
    if (i > 0) {
		i = atoi(myargv[i + 1]);
		DOOMGENERIC_RESX = SCREENWIDTH / i;
		DOOMGENERIC_RESY = SCREENHEIGHT / i;
	}

	DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);

	DG_Init();
}
