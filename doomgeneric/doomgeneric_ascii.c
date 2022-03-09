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

#include "doomkeys.h"
#include "i_system.h"
#include "m_argv.h"
#include "doomgeneric.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if defined(_WIN32) || defined(WIN32)
#define OS_WINDOWS
#include <windows.h>
//#include <synchapi.h>
#else
#define __USE_POSIX199309
#define _POSIX_C_SOURCE 199309L
#endif

#include <time.h>

#ifdef OS_WINDOWS
#define WINDOWS_CALL(cond_, format_) do {if (UNLIKELY(cond_)) winError(format_);} while (0)

void winError(char* format)
{
	LPVOID lpMsgBuf;
	DWORD dw = GetLastError();
	errno = dw;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0, NULL
	);
	I_Error(format, lpMsgBuf);
}
#endif

#define UNLIKELY(x_) __builtin_expect((x_),0)
#define CALL(stmt_, format_) do {if (UNLIKELY(stmt_)) I_Error(format_, errno);} while (0)
#define CALL_STDOUT(stmt_, format_) CALL((stmt_) == EOF, format_)

#define BYTE_TO_TEXT(buf_, byte_) do {\
	*(buf_)++ = '0' + (byte_) / 100u;\
	*(buf_)++ = '0' + (byte_) / 10u % 10u;\
	*(buf_)++ = '0' + (byte_) % 10u;\
} while (0)

const char grad[] = " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
#define GRAD_LEN 70u

struct color_t {
    uint32_t b:8;
    uint32_t g:8;
    uint32_t r:8;
    uint32_t a:8;
};

char *output_buffer;
size_t output_buffer_size;
struct timespec ts_init;

void DG_Init()
{
#ifdef OS_WINDOWS
	const HANDLE hOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	WINDOWS_CALL(hOutputHandle == INVALID_HANDLE_VALUE, "DG_Init: %s");
	DWORD mode;
	WINDOWS_CALL(!GetConsoleMode(hOutputHandle, &mode), "DG_Init: %s");
	mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	WINDOWS_CALL(!SetConsoleMode(hOutputHandle, mode), "DG_Init: %s");

	const HANDLE hInputHandle = GetStdHandle(STD_INPUT_HANDLE);
	WINDOWS_CALL(hInputHandle == INVALID_HANDLE_VALUE, "DG_Init: %s");
	WINDOWS_CALL(!GetConsoleMode(hInputHandle, &mode), "DG_Init: %s");
	mode &= ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_QUICK_EDIT_MODE);
	WINDOWS_CALL(!SetConsoleMode(hInputHandle, mode), "DG_Init: %s");
#endif
	/* Longest SGR code: \033[38;2;RRR;GGG;BBBm (length 19)
	 * Maximum 21 bytes per pixel: SGR + 2 x char
	 * 1 Newline character per line
	 * SGR clear code: \033[0m (length 4)
	 */
	 output_buffer_size = 21u * DOOMGENERIC_RESX * DOOMGENERIC_RESY + DOOMGENERIC_RESY + 4u;
	 output_buffer = malloc(output_buffer_size);

	 clock_gettime(CLOCK_REALTIME, &ts_init);
}

void DG_DrawFrame()
{
	/* clear output buffer */
	memset(output_buffer, '\0', output_buffer_size);  /* TODO: Optimize */

	/* fill output buffer */
	uint32_t color = 0xFFFFFF00;
	unsigned row, col;
	struct color_t *pixel = (struct color_t *)DG_ScreenBuffer;
	char *buf = output_buffer;

	for (row = 0; row < DOOMGENERIC_RESY; row++) {
		for (col = 0; col < DOOMGENERIC_RESX; col++) {
			if ((color ^ *(uint32_t*)pixel) & 0x00FFFFFF) {
				*buf++ = '\033'; *buf++ = '[';
				*buf++ = '3'; *buf++ = '8';
				*buf++ = ';'; *buf++ = '2';
				*buf++ = ';'; BYTE_TO_TEXT(buf, pixel->r);
				*buf++ = ';'; BYTE_TO_TEXT(buf, pixel->g);
				*buf++ = ';'; BYTE_TO_TEXT(buf, pixel->b);
				*buf++ = 'm';
				color = *(uint32_t*)pixel;
			}
			char v_char = grad[(pixel->r + pixel->g + pixel->b) * GRAD_LEN / 766u];
			*buf++ = v_char; *buf++ = v_char;
			pixel++;
		}
		*buf++ = '\n';
	}
	*buf++ = '\033'; *buf++ = '[';
	*buf++ = '0';
	*buf = 'm';

	/* clear screen */
	fputs("\033[1;1H\033[2J", stdout);

	/* flush output buffer */
	CALL_STDOUT(fputs(output_buffer, stdout), "DG_DrawFrame: fputs error %d");
}

void DG_SleepMs(uint32_t ms)
{
	#ifdef TGL_OS_WINDOWS
		Sleep(ms);
	#else
		struct timespec ts = (struct timespec) {
			.tv_sec = ms / 1000,
			.tv_nsec = (ms % 1000ul) * 1000000,
		};
		nanosleep(&ts, NULL);
	#endif
}

uint32_t DG_GetTicksMs()
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	return (ts.tv_sec - ts_init.tv_sec) * 1000 + (ts.tv_nsec - ts_init.tv_nsec) / 1000000;
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
	return 0;
}

void DG_SetWindowTitle(const char * title)
{
	(void)title;
}
