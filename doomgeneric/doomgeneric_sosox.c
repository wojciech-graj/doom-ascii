//doomgeneric for soso os (nano-x version)
//TODO: get keys from X, not using direct keyboard access!

#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <sys/ioctl.h>

#include <termios.h>

#include <soso.h>

#include <nano-X.h>


static int g_keyboard_fd = -1;

#define KEYQUEUE_SIZE 16

static unsigned short g_key_queue[KEYQUEUE_SIZE];
static unsigned int g_key_queue_write_index = 0;
static unsigned int g_key_queue_read_index = 0;


static GR_WINDOW_ID  wid;
static GR_GC_ID      gc;
static unsigned char* windowBuffer = 0;
static const int winSizeX = DOOMGENERIC_RESX;
static const int winSizeY = DOOMGENERIC_RESY;
static int button_down = 0;


static unsigned char convert_to_doom_key(unsigned char scancode)
{
    unsigned char key = 0;

    switch (scancode)
    {
    case 0x9C:
    case 0x1C:
        key = KEY_ENTER;
        break;
    case 0x01:
        key = KEY_ESCAPE;
        break;
    case 0xCB:
    case 0x4B:
        key = KEY_LEFTARROW;
        break;
    case 0xCD:
    case 0x4D:
        key = KEY_RIGHTARROW;
        break;
    case 0xC8:
    case 0x48:
        key = KEY_UPARROW;
        break;
    case 0xD0:
    case 0x50:
        key = KEY_DOWNARROW;
        break;
    case 0x1D:
        key = KEY_FIRE;
        break;
    case 0x39:
        key = KEY_USE;
        break;
    case 0x2A:
    case 0x36:
        key = KEY_RSHIFT;
        break;
    case 0x15:
        key = 'y';
        break;
    default:
        break;
    }

    return key;
}

static void add_key_to_queue(int pressed, unsigned char key_code)
{
    unsigned char key = convert_to_doom_key(key_code);

    unsigned short key_data = (pressed << 8) | key;

    g_key_queue[g_key_queue_write_index] = key_data;
    g_key_queue_write_index++;
    g_key_queue_write_index %= KEYQUEUE_SIZE;
}


struct termios orig_termios;

void disable_raw_mode()
{
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode()
{
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disable_raw_mode);
  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO);
  raw.c_cc[VMIN] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void DG_Init()
{
    if (GrOpen() < 0)
    {
        GrError("GrOpen failed");
        return;
    }

    gc = GrNewGC();
    GrSetGCUseBackground(gc, GR_FALSE);
    GrSetGCForeground(gc, MWRGB( 255, 0, 0 ));

    wid = GrNewBufferedWindow(GR_WM_PROPS_APPFRAME |
                        GR_WM_PROPS_CAPTION  |
                        GR_WM_PROPS_CLOSEBOX |
                        GR_WM_PROPS_BUFFER_MMAP |
                        GR_WM_PROPS_BUFFER_BGRA,
                        "Doom",
                        GR_ROOT_WINDOW_ID, 
                        50, 50, winSizeX, winSizeY, MWRGB( 255, 255, 255 ));

    GrSelectEvents(wid, GR_EVENT_MASK_EXPOSURE | 
                        GR_EVENT_MASK_TIMER |
                        GR_EVENT_MASK_CLOSE_REQ |
                        GR_EVENT_MASK_BUTTON_DOWN |
                        GR_EVENT_MASK_BUTTON_UP);

    GrMapWindow (wid);

    windowBuffer = GrOpenClientFramebuffer(wid);

    enable_raw_mode();

    g_keyboard_fd = open("/dev/keyboard", 0);

    if (g_keyboard_fd >= 0)
    {
        //enter non-blocking mode
        ioctl(g_keyboard_fd, 1, (void*)1);
    }
}

static void handle_key_input()
{
    if (g_keyboard_fd < 0)
    {
        return;
    }

    unsigned char scancode = 0;

    if (read(g_keyboard_fd, &scancode, 1) > 0)
    {
        unsigned char keyRelease = (0x80 & scancode);

        scancode = (0x7F & scancode);

        //printf("scancode:%x pressed:%d\n", scancode, 0 == keyRelease);

        if (0 == keyRelease)
        {
            add_key_to_queue(1, scancode);
        }
        else
        {
            add_key_to_queue(0, scancode);
        }
    }
}

void DG_DrawFrame()
{
    GR_EVENT event;
    while (GrPeekEvent(&event))
    {
        GrGetNextEvent(&event);

        switch (event.type)
        {
        case GR_EVENT_TYPE_BUTTON_DOWN:
            button_down = 1;
            break;
        case GR_EVENT_TYPE_BUTTON_UP:
            button_down = 0;
            break;

        case GR_EVENT_TYPE_CLOSE_REQ:
            GrClose();
            exit (0);
            break;
        case GR_EVENT_TYPE_EXPOSURE:
            break;
        case GR_EVENT_TYPE_TIMER:
            
            break;
        }
    }

    //if (button_down == 0)
    {
        memcpy(windowBuffer, DG_ScreenBuffer, DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);

        GrFlushWindow(wid);
    }

    handle_key_input();
}

void DG_SleepMs(uint32_t ms)
{
    sleep_ms(ms);
}

uint32_t DG_GetTicksMs()
{
    return get_uptime_ms();
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
    if (g_key_queue_read_index == g_key_queue_write_index)
    {
        //key queue is empty

        return 0;
    }
    else
    {
        unsigned short keyData = g_key_queue[g_key_queue_read_index];
        g_key_queue_read_index++;
        g_key_queue_read_index %= KEYQUEUE_SIZE;

        *pressed = keyData >> 8;
        *doomKey = keyData & 0xFF;

        return 1;
    }
}

void DG_SetWindowTitle(const char * title)
{
    GrSetWindowTitle(wid, title);
}
