//
// Copyright(C) 2022-2024 Wojciech Graj
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
#include "doomkeys.h"
#include "i_system.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(WIN32)
#define OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#endif

#ifdef OS_WINDOWS
#define CLK 0

#define WINDOWS_CALL(cond_, format_)       \
	do {                               \
		if (UNLIKELY(cond_))       \
			winError(format_); \
	} while (0)

void winError(char *format)
{
	LPVOID lpMsgBuf;
	DWORD dw = GetLastError();
	errno = dw;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);
	I_Error(format, lpMsgBuf);
}

/* Modified from https://stackoverflow.com/a/31335254 */
struct timespec {
	long tv_sec;
	long tv_nsec;
};
int clock_gettime(int p, struct timespec *spec)
{
	(void)p;
	__int64 wintime;
	GetSystemTimeAsFileTime((FILETIME *)&wintime);
	wintime -= 116444736000000000ll;
	spec->tv_sec = wintime / 10000000ll;
	spec->tv_nsec = wintime % 10000000ll * 100;
	return 0;
}

#else
#define CLK CLOCK_REALTIME
#endif

#define UNLIKELY(x_) __builtin_expect((x_), 0)
#define CALL(stmt_, format_)                     \
	do {                                     \
		if (UNLIKELY(stmt_))             \
			I_Error(format_, errno); \
	} while (0)
#define CALL_STDOUT(stmt_, format_) CALL((stmt_) == EOF, format_)

#define BYTE_TO_TEXT(buf_, byte_)                      \
	do {                                           \
		*(buf_)++ = '0' + (byte_) / 100u;      \
		*(buf_)++ = '0' + (byte_) / 10u % 10u; \
		*(buf_)++ = '0' + (byte_) % 10u;       \
	} while (0)

const char grad[] = " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
#define GRAD_LEN 70u
#define INPUT_BUFFER_LEN 16u
#define EVENT_BUFFER_LEN (INPUT_BUFFER_LEN * 2u - 1u)

struct color_t {
	uint32_t b : 8;
	uint32_t g : 8;
	uint32_t r : 8;
	uint32_t a : 8;
};

char *output_buffer;
size_t output_buffer_size;
struct timespec ts_init;

unsigned char input_buffer[INPUT_BUFFER_LEN];
uint16_t event_buffer[EVENT_BUFFER_LEN];
uint16_t *event_buf_loc;

void DG_AtExit(void)
{
#ifdef OS_WINDOWS
	DWORD mode;
	const HANDLE hInputHandle = GetStdHandle(STD_INPUT_HANDLE);
	if (UNLIKELY(hInputHandle == INVALID_HANDLE_VALUE))
		return;
	if (UNLIKELY(!GetConsoleMode(hInputHandle, &mode)))
		return;
	mode |= ENABLE_ECHO_INPUT;
	SetConsoleMode(hInputHandle, mode);
#else
	struct termios t;
	if (UNLIKELY(tcgetattr(STDIN_FILENO, &t)))
		return;
	t.c_lflag |= ECHO;
	tcsetattr(STDIN_FILENO, TCSANOW, &t);
#endif
}

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
	mode &= ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_QUICK_EDIT_MODE | ENABLE_ECHO_INPUT);
	WINDOWS_CALL(!SetConsoleMode(hInputHandle, mode), "DG_Init: %s");
#else
	struct termios t;
	CALL(tcgetattr(STDIN_FILENO, &t), "DG_Init: tcgetattr error %d");
	t.c_lflag &= ~(ECHO);
	CALL(tcsetattr(STDIN_FILENO, TCSANOW, &t), "DG_Init: tcsetattr error %d");
#endif
	atexit(&DG_AtExit);

	/* Longest SGR code: \033[38;2;RRR;GGG;BBBm (length 19)
	 * Maximum 21 bytes per pixel: SGR + 2 x char
	 * 1 Newline character per line
	 * SGR clear code: \033[0m (length 4)
	 */
	output_buffer_size = 21u * DOOMGENERIC_RESX * DOOMGENERIC_RESY + DOOMGENERIC_RESY + 4u;
	output_buffer = malloc(output_buffer_size);

	clock_gettime(CLK, &ts_init);

	memset(input_buffer, '\0', INPUT_BUFFER_LEN);
}

void DG_DrawFrame()
{
	/* Clear screen if first frame */
	static bool first_frame = true;
	if (first_frame) {
		first_frame = false;
		fputs("\033[1;1H\033[2J", stdout);
	}

	/* fill output buffer */
	uint32_t color = 0xFFFFFF00;
	unsigned row, col;
	struct color_t *pixel = (struct color_t *)DG_ScreenBuffer;
	char *buf = output_buffer;

	for (row = 0; row < DOOMGENERIC_RESY; row++) {
		for (col = 0; col < DOOMGENERIC_RESX; col++) {
			if ((color ^ *(uint32_t *)pixel) & 0x00FFFFFF) {
				*buf++ = '\033';
				*buf++ = '[';
				*buf++ = '3';
				*buf++ = '8';
				*buf++ = ';';
				*buf++ = '2';
				*buf++ = ';';
				BYTE_TO_TEXT(buf, pixel->r);
				*buf++ = ';';
				BYTE_TO_TEXT(buf, pixel->g);
				*buf++ = ';';
				BYTE_TO_TEXT(buf, pixel->b);
				*buf++ = 'm';
				color = *(uint32_t *)pixel;
			}
			char v_char = grad[(pixel->r + pixel->g + pixel->b) * GRAD_LEN / 766u];
			*buf++ = v_char;
			*buf++ = v_char;
			pixel++;
		}
		*buf++ = '\n';
	}
	*buf++ = '\033';
	*buf++ = '[';
	*buf++ = '0';
	*buf = 'm';

	/* move cursor to top left corner and set bold text*/
	fputs("\033[;H\033[1m", stdout);

	/* flush output buffer */
	CALL_STDOUT(fputs(output_buffer, stdout), "DG_DrawFrame: fputs error %d");

	/* clear output buffer */
	memset(output_buffer, '\0', buf - output_buffer + 1u);
}

void DG_SleepMs(uint32_t ms)
{
#ifdef OS_WINDOWS
	Sleep(ms);
#else
	struct timespec ts = (struct timespec){
		.tv_sec = ms / 1000,
		.tv_nsec = (ms % 1000ul) * 1000000,
	};
	nanosleep(&ts, NULL);
#endif
}

uint32_t DG_GetTicksMs()
{
	struct timespec ts;
	clock_gettime(CLK, &ts);

	return (ts.tv_sec - ts_init.tv_sec) * 1000 + (ts.tv_nsec - ts_init.tv_nsec) / 1000000;
}

#ifdef OS_WINDOWS
unsigned char convertToDoomKey(WORD wVirtualKeyCode, CHAR AsciiChar)
{
	switch (wVirtualKeyCode) {
	case VK_RETURN:
		return KEY_ENTER;
	case VK_LEFT:
		return KEY_LEFTARROW;
	case VK_UP:
		return KEY_UPARROW;
	case VK_RIGHT:
		return KEY_RIGHTARROW;
	case VK_DOWN:
		return KEY_DOWNARROW;
	case VK_SPACE:
		return KEY_FIRE;
	case VK_TAB:
		return KEY_TAB;
	case VK_F1:
		return KEY_F1;
	case VK_F2:
		return KEY_F2;
	case VK_F3:
		return KEY_F3;
	case VK_F4:
		return KEY_F4;
	case VK_F5:
		return KEY_F5;
	case VK_F6:
		return KEY_F6;
	case VK_F7:
		return KEY_F7;
	case VK_F8:
		return KEY_F8;
	case VK_F9:
		return KEY_F9;
	case VK_F10:
		return KEY_F10;
	case VK_F11:
		return KEY_F11;
	case VK_F12:
		return KEY_F12;
	case VK_BACK:
		return KEY_BACKSPACE;
	case VK_PAUSE:
		return KEY_PAUSE;
	case VK_RSHIFT:
		return KEY_RSHIFT;
	case VK_RCONTROL:
		return KEY_RCTRL;
	case VK_CAPITAL:
		return KEY_CAPSLOCK;
	case VK_NUMLOCK:
		return KEY_NUMLOCK;
	case VK_SCROLL:
		return KEY_SCRLCK;
	case VK_SNAPSHOT:
		return KEY_PRTSCR;
	case VK_HOME:
		return KEY_HOME;
	case VK_END:
		return KEY_END;
	case VK_PRIOR:
		return KEY_PGUP;
	case VK_NEXT:
		return KEY_PGDN;
	case VK_INSERT:
		return KEY_INS;
	case VK_DELETE:
		return KEY_DEL;
	case VK_NUMPAD0:
		return KEYP_0;
	case VK_NUMPAD1:
		return KEYP_1;
	case VK_NUMPAD2:
		return KEYP_2;
	case VK_NUMPAD3:
		return KEYP_3;
	case VK_NUMPAD4:
		return KEYP_4;
	case VK_NUMPAD5:
		return KEYP_5;
	case VK_NUMPAD6:
		return KEYP_6;
	case VK_NUMPAD7:
		return KEYP_7;
	case VK_NUMPAD8:
		return KEYP_8;
	case VK_NUMPAD9:
		return KEYP_9;
	default:
		return tolower(AsciiChar);
	}
}
#else
unsigned char convertToDoomKey(char **buf)
{
	switch (**buf) {
	case '\012':
		return KEY_ENTER;
	case '\033':
		(*buf)++;
		switch (**buf) {
		case '[':
			(*buf)++;
			switch (**buf) {
			case 'A':
				return KEY_UPARROW;
			case 'B':
				return KEY_DOWNARROW;
			case 'C':
				return KEY_RIGHTARROW;
			case 'D':
				return KEY_LEFTARROW;
			case 'H':
				return KEY_HOME;
			case 'F':
				return KEY_END;
			case '1':
				(*buf)++;
				switch (**buf) {
				case '5':
					return (*(++(*buf)) == '~') ? KEY_F5 : '\0';
				case '7':
					return (*(++(*buf)) == '~') ? KEY_F6 : '\0';
				case '8':
					return (*(++(*buf)) == '~') ? KEY_F7 : '\0';
				case '9':
					return (*(++(*buf)) == '~') ? KEY_F8 : '\0';
				default:
					return '\0';
				}
			case '2':
				(*buf)++;
				switch (**buf) {
				case '0':
					return (*(++(*buf)) == '~') ? KEY_F9 : '\0';
				case '1':
					return (*(++(*buf)) == '~') ? KEY_F10 : '\0';
				case '3':
					return (*(++(*buf)) == '~') ? KEY_F11 : '\0';
				case '4':
					return (*(++(*buf)) == '~') ? KEY_F12 : '\0';
				case '~':
					return KEY_INS;
				default:
					return '\0';
				}
			case '3':
				return (*(++(*buf)) == '~') ? KEY_DEL : '\0';
			case '5':
				return (*(++(*buf)) == '~') ? KEY_PGUP : '\0';
			case '6':
				return (*(++(*buf)) == '~') ? KEY_PGDN : '\0';
			default:
				return '\0';
			}
		case 'O':
			(*buf)++;
			switch (**buf) {
			case 'P':
				return KEY_F1;
			case 'Q':
				return KEY_F2;
			case 'R':
				return KEY_F3;
			case 'S':
				return KEY_F4;
			default:
				return '\0';
			}
		default:
			(*buf)--;
			return KEY_ESCAPE;
		}
	case ' ':
		return KEY_FIRE;
	default:
		return tolower(**buf);
	}
}
#endif

void DG_ReadInput(void)
{
	static unsigned char prev_input_buffer[INPUT_BUFFER_LEN];

	memcpy(prev_input_buffer, input_buffer, INPUT_BUFFER_LEN);
	memset(input_buffer, '\0', INPUT_BUFFER_LEN);
	memset(event_buffer, '\0', 2u * EVENT_BUFFER_LEN);
	event_buf_loc = event_buffer;
#ifdef OS_WINDOWS
	const HANDLE hInputHandle = GetStdHandle(STD_INPUT_HANDLE);
	WINDOWS_CALL(hInputHandle == INVALID_HANDLE_VALUE, "DG_ReadInput: %s");

	/* Disable canonical mode */
	DWORD old_mode, new_mode;
	WINDOWS_CALL(!GetConsoleMode(hInputHandle, &old_mode), "DG_ReadInput: %s");
	new_mode = old_mode;
	new_mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
	WINDOWS_CALL(!SetConsoleMode(hInputHandle, new_mode), "DG_ReadInput: %s");

	DWORD event_cnt;
	WINDOWS_CALL(!GetNumberOfConsoleInputEvents(hInputHandle, &event_cnt), "DG_ReadInput: %s");

	/* ReadConsole is blocking so must manually process events */
	int input_count = 0;
	if (event_cnt) {
		INPUT_RECORD input_records[32];
		WINDOWS_CALL(!ReadConsoleInput(hInputHandle, input_records, 32, &event_cnt), "DG_ReadInput: %s");

		DWORD i;
		for (i = 0; i < event_cnt; i++) {
			if (input_records[i].Event.KeyEvent.bKeyDown && input_records[i].EventType == KEY_EVENT) {
				unsigned char inp = convertToDoomKey(input_records[i].Event.KeyEvent.wVirtualKeyCode, input_records[i].Event.KeyEvent.uChar.AsciiChar);
				if (inp) {
					input_buffer[input_count++] = inp;
					if (input_count == INPUT_BUFFER_LEN - 1u)
						break;
				}
			}
		}
	}

	WINDOWS_CALL(!SetConsoleMode(hInputHandle, old_mode), "DG_ReadInput: %s");
#else /* defined(OS_WINDOWS) */
	static char raw_input_buffer[INPUT_BUFFER_LEN];
	struct termios oldt, newt;

	memset(raw_input_buffer, '\0', INPUT_BUFFER_LEN);

	/* Disable canonical mode */
	CALL(tcgetattr(STDIN_FILENO, &oldt), "DG_DrawFrame: tcgetattr error %d");
	newt = oldt;
	newt.c_lflag &= ~(ICANON);
	newt.c_cc[VMIN] = 0;
	newt.c_cc[VTIME] = 0;
	CALL(tcsetattr(STDIN_FILENO, TCSANOW, &newt), "DG_DrawFrame: tcsetattr error %d");

	CALL(read(2, raw_input_buffer, INPUT_BUFFER_LEN - 1u) < 0, "DG_DrawFrame: read error %d");

	CALL(tcsetattr(STDIN_FILENO, TCSANOW, &oldt), "DG_DrawFrame: tcsetattr error %d");

	/* Flush input buffer to prevent read of previous unread input */
	CALL(tcflush(STDIN_FILENO, TCIFLUSH), "DG_DrawFrame: tcflush error %d");

	/* create input buffer */
	char *raw_input_buf_loc = raw_input_buffer;
	unsigned char *input_buf_loc = input_buffer;
	while (*raw_input_buf_loc) {
		unsigned char inp = convertToDoomKey(&raw_input_buf_loc);
		if (!inp)
			break;
		*input_buf_loc++ = inp;
		raw_input_buf_loc++;
	}
#endif
	/* construct event array */
	int i, j;
	for (i = 0; input_buffer[i]; i++) {
		/* skip duplicates */
		for (j = i + 1; input_buffer[j]; j++) {
			if (input_buffer[i] == input_buffer[j])
				goto LBL_CONTINUE_1;
		}

		/* pressed events */
		for (j = 0; prev_input_buffer[j]; j++) {
			if (input_buffer[i] == prev_input_buffer[j])
				goto LBL_CONTINUE_1;
		}
		*event_buf_loc++ = 0x0100 | input_buffer[i];

	LBL_CONTINUE_1:;
	}

	/* depressed events */
	for (i = 0; prev_input_buffer[i]; i++) {
		for (j = 0; input_buffer[j]; j++) {
			if (prev_input_buffer[i] == input_buffer[j])
				goto LBL_CONTINUE_2;
		}
		*event_buf_loc++ = 0xFF & prev_input_buffer[i];

	LBL_CONTINUE_2:;
	}

	event_buf_loc = event_buffer;
}

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
	if (!*event_buf_loc)
		return 0;

	*pressed = *event_buf_loc >> 8;
	*doomKey = *event_buf_loc & 0xFF;
	event_buf_loc++;
	return 1;
}

void DG_SetWindowTitle(const char *title)
{
	(void)title;
}
