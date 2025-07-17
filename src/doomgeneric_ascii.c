//
// Copyright(C) 2022-2025 Wojciech Graj
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
#include "m_argv.h"

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(WIN32)
#define OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <termios.h>
#include <time.h>
#include <unistd.h>
#endif

#ifdef OS_WINDOWS
#define CLK 0
#define dg_random rand

#define WINDOWS_CALL(cond, format)                                                                 \
	do {                                                                                       \
		if (UNLIKELY(cond))                                                                \
			winError(format);                                                          \
	} while (0)

static void winError(char *const format)
{
	LPVOID lpMsgBuf;
	const DWORD dw = GetLastError();
	errno = dw;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
			| FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
	I_Error(format, lpMsgBuf);
}

/* Modified from https://stackoverflow.com/a/31335254 */
struct timespec {
	long tv_sec;
	long tv_nsec;
};

static int clock_gettime(const int p, struct timespec *const spec)
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
#define dg_random random
#endif

#ifdef __GNUC__
#define UNLIKELY(x) __builtin_expect((x), 0)
#else
#define UNLIKELY(x) (x)
#endif

#define CALL(stmt, format)                                                                         \
	do {                                                                                       \
		if (UNLIKELY(stmt))                                                                \
			I_Error(format, errno);                                                    \
	} while (0)
#define CALL_STDOUT(stmt, format) CALL((stmt) == EOF, format)

#define BUF_ITOA(buf, byte)                                                                        \
	do {                                                                                       \
		*(buf)++ = '0' + (byte) / 100u;                                                    \
		*(buf)++ = '0' + (byte) / 10u % 10u;                                               \
		*(buf)++ = '0' + (byte) % 10u;                                                     \
	} while (0)
#define BUF_MEMCPY(buf, s, len)                                                                    \
	do {                                                                                       \
		memcpy(buf, s, len);                                                               \
		(buf) += (len);                                                                    \
	} while (0)
#define BUF_PUTS(buf, s)                                                                           \
	do {                                                                                       \
		BUF_MEMCPY(buf, s, static_strlen(s));                                              \
	} while (0)
#define BUF_PUTCHAR(buf, c)                                                                        \
	do {                                                                                       \
		*(buf)++ = c;                                                                      \
	} while (0)

#define static_strlen(s) (sizeof(s) - 1)

enum {
	UNICODE_GRAD_LEN = 4U,
	INPUT_BUFFER_LEN = 16U,
	EVENT_BUFFER_LEN = 257U,
	RGB_SUM_MAX = 776U,
	DEMO_MAX_MS = 600000U,
};

static const char grad[] =
	" .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
static const char unicode_grad[] = "\u2591\u2592\u2593\u2588";
static const char *braille_grads[] = { "\u2801\u2802\u2804\u2808\u2810\u2820\u2840\u2880",
	"\u2803\u2805\u2806\u2809\u280a\u280c\u2811\u2812\u2814\u2818\u2821\u2822\u2824\u2828\u2830\u2841\u2842\u2844\u2848\u2850\u2860\u2881\u2882\u2884\u2888\u2890\u28a0\u28c0",
	"\u2807\u280b\u280d\u280e\u2813\u2815\u2816\u2819\u281a\u281c\u2823\u2825\u2826\u2829\u282a\u282c\u2831\u2832\u2834\u2838\u2843\u2845\u2846\u2849\u284a\u284c\u2851\u2852\u2854\u2858\u2861\u2862\u2864\u2868\u2870\u2883\u2885\u2886\u2889\u288a\u288c\u2891\u2892\u2894\u2898\u28a1\u28a2\u28a4\u28a8\u28b0\u28c1\u28c2\u28c4\u28c8\u28d0\u28e0",
	"\u280f\u2817\u281b\u281d\u281e\u2827\u282b\u282d\u282e\u2833\u2835\u2836\u2839\u283a\u283c\u2847\u284b\u284d\u284e\u2853\u2855\u2856\u2859\u285a\u285c\u2863\u2865\u2866\u2869\u286a\u286c\u2871\u2872\u2874\u2878\u2887\u288b\u288d\u288e\u2893\u2895\u2896\u2899\u289a\u289c\u28a3\u28a5\u28a6\u28a9\u28aa\u28ac\u28b1\u28b2\u28b4\u28b8\u28c3\u28c5\u28c6\u28c9\u28ca\u28cc\u28d1\u28d2\u28d4\u28d8\u28e1\u28e2\u28e4\u28e8\u28f0",
	"\u281f\u282f\u2837\u283b\u283d\u283e\u284f\u2857\u285b\u285d\u285e\u2867\u286b\u286d\u286e\u2873\u2875\u2876\u2879\u287a\u287c\u288f\u2897\u289b\u289d\u289e\u28a7\u28ab\u28ad\u28ae\u28b3\u28b5\u28b6\u28b9\u28ba\u28bc\u28c7\u28cb\u28cd\u28ce\u28d3\u28d5\u28d6\u28d9\u28da\u28dc\u28e3\u28e5\u28e6\u28e9\u28ea\u28ec\u28f1\u28f2\u28f4\u28f8",
	"\u283f\u285f\u286f\u2877\u287b\u287d\u287e\u289f\u28af\u28b7\u28bb\u28bd\u28be\u28cf\u28d7\u28db\u28dd\u28de\u28e7\u28eb\u28ed\u28ee\u28f3\u28f5\u28f6\u28f9\u28fa\u28fc",
	"\u287f\u28bf\u28df\u28ef\u28f7\u28fb\u28fd\u28fe", "\u28ff" };
static const size_t braille_grad_lengths[] = { static_strlen(braille_grads[0]),
	static_strlen(braille_grads[1]), static_strlen(braille_grads[2]),
	static_strlen(braille_grads[3]), static_strlen(braille_grads[4]),
	static_strlen(braille_grads[5]), static_strlen(braille_grads[6]),
	static_strlen(braille_grads[7]) };

/* print(','.join(str(int(((i / 255)**0.5) * 255)) for i in range(256))) */
static const uint8_t byte_sqrt[] = { 0, 15, 22, 27, 31, 35, 39, 42, 45, 47, 50, 52, 55, 57, 59, 61,
	63, 65, 67, 69, 71, 73, 74, 76, 78, 79, 81, 82, 84, 85, 87, 88, 90, 91, 93, 94, 95, 97, 98,
	99, 100, 102, 103, 104, 105, 107, 108, 109, 110, 111, 112, 114, 115, 116, 117, 118, 119,
	120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137,
	138, 139, 140, 141, 141, 142, 143, 144, 145, 146, 147, 148, 148, 149, 150, 151, 152, 153,
	153, 154, 155, 156, 157, 158, 158, 159, 160, 161, 162, 162, 163, 164, 165, 165, 166, 167,
	168, 168, 169, 170, 171, 171, 172, 173, 174, 174, 175, 176, 177, 177, 178, 179, 179, 180,
	181, 182, 182, 183, 184, 184, 185, 186, 186, 187, 188, 188, 189, 190, 190, 191, 192, 192,
	193, 194, 194, 195, 196, 196, 197, 198, 198, 199, 200, 200, 201, 201, 202, 203, 203, 204,
	205, 205, 206, 206, 207, 208, 208, 209, 210, 210, 211, 211, 212, 213, 213, 214, 214, 215,
	216, 216, 217, 217, 218, 218, 219, 220, 220, 221, 221, 222, 222, 223, 224, 224, 225, 225,
	226, 226, 227, 228, 228, 229, 229, 230, 230, 231, 231, 232, 233, 233, 234, 234, 235, 235,
	236, 236, 237, 237, 238, 238, 239, 240, 240, 241, 241, 242, 242, 243, 243, 244, 244, 245,
	245, 246, 246, 247, 247, 248, 248, 249, 249, 250, 250, 251, 251, 252, 252, 253, 253, 254,
	255 };

struct color_t {
	uint32_t b : 8;
	uint32_t g : 8;
	uint32_t r : 8;
	uint32_t a : 8;
};

struct event_buffer_t {
	bool pressed;
	unsigned char key;
};

enum character_set_t { ASCII, BLOCK, BRAILLE };

static char *output_buffer;
static size_t output_buffer_size;
static struct timespec ts_init;

static struct timespec input_buffer[256] = { 0 };
static struct event_buffer_t event_buffer[EVENT_BUFFER_LEN] = { 0 };
static struct event_buffer_t *event_buf_loc;

static bool color_enabled;
static enum character_set_t character_set = ASCII;
static bool gradient_enabled;
static bool bold_enabled;
static bool erase_enabled;
static bool gamma_correct_enabled;
static unsigned keypress_smoothing_ms = 42;

static int64_t sub_timespec_ms(
	const struct timespec *const time1, const struct timespec *const time0)
{
	return (time1->tv_sec - time0->tv_sec) * 1000L
		+ (time1->tv_nsec - time0->tv_nsec) / 1000000L;
}

void DG_AtExit(void)
{
	if (color_enabled || bold_enabled)
		(void)fputs("\033[0m", stdout);

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

void DG_Init(void)
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
	mode &= ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_QUICK_EDIT_MODE
		| ENABLE_ECHO_INPUT);
	WINDOWS_CALL(!SetConsoleMode(hInputHandle, mode), "DG_Init: %s");
#else
	struct termios t;
	CALL(tcgetattr(STDIN_FILENO, &t), "DG_Init: tcgetattr error %d");
	t.c_lflag &= ~(ECHO);
	CALL(tcsetattr(STDIN_FILENO, TCSANOW, &t), "DG_Init: tcsetattr error %d");
#endif
	CALL(atexit(&DG_AtExit), "DG_Init: atexit error %d");

	color_enabled = M_CheckParm("-nocolor") == 0;
	gradient_enabled = M_CheckParm("-nograd") == 0;
	bold_enabled = M_CheckParm("-nobold") == 0;
	erase_enabled = M_CheckParm("-erase") > 0;
	gamma_correct_enabled = M_CheckParm("-fixgamma") > 0;

	int i = M_CheckParmWithArgs("-chars", 1);
	if (i > 0) {
		if (!strcmp("ascii", myargv[i + 1])) {
			character_set = ASCII;
		} else if (!strcmp("block", myargv[i + 1])) {
			character_set = BLOCK;
		} else if (!strcmp("braille", myargv[i + 1])) {
			character_set = BRAILLE;
		} else {
			I_Error("Unrecognized argument for -chars: '%s'", myargv[i + 1]);
		}
	}

	i = M_CheckParmWithArgs("-kpsmooth", 1);
	if (i > 0)
		keypress_smoothing_ms = atoi(myargv[i + 1]);

	if (character_set != ASCII) {
#ifdef OS_WINDOWS
		WINDOWS_CALL(!SetConsoleOutputCP(CP_UTF8), "DG_Init: %s");
#else
		if (!setlocale(LC_ALL, "en_US.UTF-8"))
			I_Error("DG_Init: setlocale error");
#endif
	}

	/* Longest per-pixel SGR code: \033[38;2;RRR;GGG;BBBm (length 19)
	 * 2 Chars per pixel
	 * 1 Newline character per line
	 * 1 NUL terminator
	 * SGR move cursor code: \033[;H (length 4)
	 * SGR clear code: \033[0m (length 4)
	 * SGR bold code: \033[1m (length 4)
	 * SGR erase code: \033[2J (length 4)
	 */
	output_buffer_size = ((color_enabled ? 19U : 0U) + (character_set == ASCII ? 2U : 6U))
			* DOOMGENERIC_RESX * DOOMGENERIC_RESY
		+ DOOMGENERIC_RESY + 1U + 4U + (bold_enabled ? 4U : 0U) + (erase_enabled ? 4U : 0U)
		+ ((color_enabled || bold_enabled) ? 4U : 0U);
	output_buffer = malloc(output_buffer_size);

	CALL(clock_gettime(CLK, &ts_init), "DG_Init: clock_gettime error %d");
}

void DG_DrawFrame(void)
{
	/* Clear screen if first frame */
	static bool first_frame = true;
	if (first_frame) {
		first_frame = false;
		CALL_STDOUT(fputs("\033[1;1H\033[2J", stdout), "DG_DrawFrame: fputs error %d");
	}

#ifdef DG_DEMO
	struct timespec now;
	CALL(clock_gettime(CLK, &now), "DG_DrawFrame: clock_gettime error %d");
	if (sub_timespec_ms(&now, &ts_init) > DEMO_MAX_MS) {
		puts("\033[;H\033[2JThe telnet demo of doom-ascii is limited to 10 minutes, as computational\nresources don't grow on trees. Thank you for playing!\n- Wojciech Graj <me@w-graj.net>");
		exit(0);
	}
#endif /* DG_DEMO */

	uint32_t color = 0x00FFFFFF;
	unsigned row, col;
	struct color_t *pixel = (struct color_t *)DG_ScreenBuffer;
	char *buf = output_buffer;

	/* fill output buffer */
	BUF_PUTS(buf, "\033[;H"); /* move cursor to top left corner */
	if (erase_enabled)
		BUF_PUTS(buf, "\033[2J");
	if (bold_enabled)
		BUF_PUTS(buf, "\033[1m");
	for (row = 0; row < DOOMGENERIC_RESY; row++) {
		for (col = 0; col < DOOMGENERIC_RESX; col++) {
			if (gamma_correct_enabled) {
				pixel->r = byte_sqrt[pixel->r];
				pixel->g = byte_sqrt[pixel->g];
				pixel->b = byte_sqrt[pixel->b];
			}

			if (color_enabled && (color ^ *(uint32_t *)pixel) & 0x00FFFFFF) {
				BUF_PUTS(buf, "\033[38;2;");
				BUF_ITOA(buf, pixel->r);
				BUF_PUTCHAR(buf, ';');
				BUF_ITOA(buf, pixel->g);
				BUF_PUTCHAR(buf, ';');
				BUF_ITOA(buf, pixel->b);
				BUF_PUTCHAR(buf, 'm');
				color = *(uint32_t *)pixel;
			}

			switch (character_set) {
			case ASCII:
				if (gradient_enabled) {
					const char v_char = grad[(pixel->r + pixel->g + pixel->b)
						* static_strlen(grad) / RGB_SUM_MAX];
					BUF_PUTCHAR(buf, v_char);
					BUF_PUTCHAR(buf, v_char);
				} else {
					BUF_PUTS(buf, "##");
				}
				break;
			case BLOCK:
				if (gradient_enabled) {
					const size_t idx = (pixel->r + pixel->g + pixel->b)
						* (UNICODE_GRAD_LEN + 1U) / RGB_SUM_MAX;
					if (idx) {
						const void *const v_char =
							&unicode_grad[(idx - 1) * 3];
						BUF_MEMCPY(buf, v_char, 3);
						BUF_MEMCPY(buf, v_char, 3);
					} else {
						BUF_PUTS(buf, "  ");
					}
				} else {
					BUF_PUTS(buf, "\u2588\u2588");
				}
				break;
			case BRAILLE:
				if (gradient_enabled) {
					const size_t idx =
						(pixel->r + pixel->g + pixel->b) * 8 / RGB_SUM_MAX;
					if (idx) {
						const char *const gradient = braille_grads[idx - 1];
						const size_t len =
							braille_grad_lengths[idx - 1] / 3;
						BUF_MEMCPY(
							buf, &gradient[(dg_random() % len) * 3], 3);
						BUF_MEMCPY(
							buf, &gradient[(dg_random() % len) * 3], 3);
					} else {
						BUF_PUTS(buf, "  ");
					}
				} else {
					BUF_PUTS(buf, "\u28ff\u28ff");
				}
				break;
			}

			pixel++;
		}
		BUF_PUTCHAR(buf, '\n');
	}
	if (color_enabled || bold_enabled)
		BUF_PUTS(buf, "\033[0m");
	BUF_PUTCHAR(buf, '\0');

	CALL_STDOUT(fputs(output_buffer, stdout), "DG_DrawFrame: fputs error %d");
}

void DG_SleepMs(const uint32_t ms)
{
#ifdef OS_WINDOWS
	Sleep(ms);
#else
	const struct timespec ts = (struct timespec){
		.tv_sec = ms / 1000,
		.tv_nsec = (ms % 1000) * 1000000L,
	};
	nanosleep(&ts, NULL);
#endif
}

uint32_t DG_GetTicksMs(void)
{
	struct timespec ts;
	CALL(clock_gettime(CLK, &ts), "DG_GetTickMs: clock_gettime error: %d");

	return (ts.tv_sec - ts_init.tv_sec) * 1000 + (ts.tv_nsec - ts_init.tv_nsec) / 1000000;
}

#ifdef OS_WINDOWS
static inline unsigned char convertToDoomKey(const WORD wVirtualKeyCode, const CHAR AsciiChar)
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
static unsigned char doomKeyIfTilda(const char **const buf, const unsigned char key)
{
	if (*((*buf) + 1) != '~')
		return '\0';
	(*buf)++;
	return key;
}

static inline unsigned char convertCsiToDoomKey(const char **const buf)
{
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
		switch (*((*buf) + 1)) {
		case '5':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F5);
		case '7':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F6);
		case '8':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F7);
		case '9':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F8);
		default:
			return '\0';
		}
	case '2':
		switch (*((*buf) + 1)) {
		case '0':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F9);
		case '1':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F10);
		case '3':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F11);
		case '4':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F12);
		case '~':
			(*buf)++;
			return KEY_INS;
		default:
			return '\0';
		}
	case '3':
		return doomKeyIfTilda(buf, KEY_DEL);
	case '5':
		return doomKeyIfTilda(buf, KEY_PGUP);
	case '6':
		return doomKeyIfTilda(buf, KEY_PGDN);
	default:
		return '\0';
	}
}

static inline unsigned char convertSs3ToDoomKey(const char **const buf)
{
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
}

static inline unsigned char convertToDoomKey(const char **const buf)
{
	switch (**buf) {
	case '\012':
		return KEY_ENTER;
	case '\033':
		switch (*((*buf) + 1)) {
		case '[':
			*buf += 2;
			return convertCsiToDoomKey(buf);
		case 'O':
			*buf += 2;
			return convertSs3ToDoomKey(buf);
		default:
			return KEY_ESCAPE;
		}
	default:
		return tolower(**buf);
	}
}
#endif

void DG_ReadInput(void)
{
	struct timespec prev_input_buffer[256];
	memcpy(prev_input_buffer, input_buffer, sizeof(struct timespec[256]));

	struct timespec now;
	CALL(clock_gettime(CLK, &now), "DG_ReadInput: clock_gettime error %d");
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
	if (event_cnt) {
		INPUT_RECORD input_records[32];
		WINDOWS_CALL(!ReadConsoleInput(hInputHandle, input_records, 32, &event_cnt),
			"DG_ReadInput: %s");

		DWORD i;
		for (i = 0; i < event_cnt; i++) {
			if (input_records[i].Event.KeyEvent.bKeyDown
				&& input_records[i].EventType == KEY_EVENT) {
				unsigned char inp = convertToDoomKey(
					input_records[i].Event.KeyEvent.wVirtualKeyCode,
					input_records[i].Event.KeyEvent.uChar.AsciiChar);
				input_buffer[inp] = now;
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

	CALL(read(2, raw_input_buffer, INPUT_BUFFER_LEN - 1U) < 0, "DG_DrawFrame: read error %d");

	CALL(tcsetattr(STDIN_FILENO, TCSANOW, &oldt), "DG_DrawFrame: tcsetattr error %d");

	/* Flush input buffer to prevent read of previous unread input */
	CALL(tcflush(STDIN_FILENO, TCIFLUSH), "DG_DrawFrame: tcflush error %d");

	/* create input buffer */
	const char *raw_input_buf_loc = raw_input_buffer;
	while (*raw_input_buf_loc) {
		const unsigned char inp = convertToDoomKey(&raw_input_buf_loc);
		input_buffer[inp] = now;
		raw_input_buf_loc++;
	}
#endif
	memset(event_buffer, '\0', sizeof(struct event_buffer_t[EVENT_BUFFER_LEN]));
	event_buf_loc = event_buffer;

	int i;
	for (i = 1; i < 256; i++) {
		if (!memcmp(&input_buffer[i], &(struct timespec){ 0 }, sizeof(struct timespec)))
			continue;

		/* depressed events */
		if (sub_timespec_ms(&now, &input_buffer[i]) > keypress_smoothing_ms) {
			input_buffer[i] = (struct timespec){ 0 };
			*event_buf_loc++ = (struct event_buffer_t){ .key = i };
			continue;
		}

		/* pressed events */
		if (!memcmp(&prev_input_buffer[i], &(struct timespec){ 0 },
			    sizeof(struct timespec)))
			*event_buf_loc++ = (struct event_buffer_t){ .key = i, .pressed = true };
	}

	event_buf_loc = event_buffer;
}

int DG_GetKey(int *const pressed, unsigned char *const doomKey)
{
	if (!event_buf_loc->key)
		return 0;

	*pressed = event_buf_loc->pressed;
	*doomKey = event_buf_loc->key;
	event_buf_loc++;
	return 1;
}

void DG_SetWindowTitle(const char *const title)
{
	CALL_STDOUT(fputs("\033]2;", stdout), "DG_SetWindowTitle: fputs error %d");
	CALL_STDOUT(fputs(title, stdout), "DG_SetWindowTitle: fputs error %d");
	CALL_STDOUT(fputs("\033\\", stdout), "DG_SetWindowTitle: fputs error %d");
}
