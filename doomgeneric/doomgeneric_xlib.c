#include "doomkeys.h"

#include "doomgeneric.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

static Display *s_Display = NULL;
static Window s_Window = NULL;
static int s_Screen = 0;
static GC s_Gc = 0;
static Pixmap s_Pixmap = NULL;

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned char convertToDoomKey(unsigned int key)
{
	switch (key)
	{
    case XK_Return:
		key = KEY_ENTER;
		break;
    case XK_Escape:
		key = KEY_ESCAPE;
		break;
    case XK_Left:
		key = KEY_LEFTARROW;
		break;
    case XK_Right:
		key = KEY_RIGHTARROW;
		break;
    case XK_Up:
		key = KEY_UPARROW;
		break;
    case XK_Down:
		key = KEY_DOWNARROW;
		break;
    case XK_Control_L:
    case XK_Control_R:
		key = KEY_FIRE;
		break;
    case XK_space:
		key = KEY_USE;
		break;
    case XK_Shift_L:
    case XK_Shift_R:
		key = KEY_RSHIFT;
		break;
	default:
		key = tolower(key);
		break;
	}

	return key;
}

static void addKeyToQueue(int pressed, unsigned int keyCode)
{
	unsigned char key = convertToDoomKey(keyCode);

	unsigned short keyData = (pressed << 8) | key;

	s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
	s_KeyQueueWriteIndex++;
	s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

void DG_Init()
{
	memset(s_KeyQueue, 0, KEYQUEUE_SIZE * sizeof(unsigned short));

    // window creation

    s_Display = XOpenDisplay(NULL);

    s_Screen = DefaultScreen(s_Display);

    int blackColor = BlackPixel(s_Display, s_Screen);
    int whiteColor = WhitePixel(s_Display, s_Screen);

    XSetWindowAttributes attr;
    memset(&attr, 0, sizeof(XSetWindowAttributes));
    attr.event_mask = ExposureMask | KeyPressMask;
    attr.background_pixel = BlackPixel(s_Display, s_Screen);

    int depth = DefaultDepth(s_Display, s_Screen);

    s_Window = XCreateSimpleWindow(s_Display, DefaultRootWindow(s_Display), 0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY, 0, blackColor, blackColor);

    XSelectInput(s_Display, s_Window, StructureNotifyMask | KeyPressMask | KeyReleaseMask);

    XMapWindow(s_Display, s_Window);

    s_Gc = XCreateGC(s_Display, s_Window, 0, NULL);

    XSetForeground(s_Display, s_Gc, whiteColor);

    XkbSetDetectableAutoRepeat(s_Display, 1, 0);

    // Wait for the MapNotify event

    while(1)
    {
        XEvent e;
        XNextEvent(s_Display, &e);
        if (e.type == MapNotify)
        {
            break;
        }
    }

    s_Pixmap = XCreatePixmap(s_Display, s_Window, DOOMGENERIC_RESX, DOOMGENERIC_RESY, depth);
}


void DG_DrawFrame()
{
    if (s_Display)
    {
        while (XPending(s_Display) > 0)
        {
            XEvent e;
            XNextEvent(s_Display, &e);
            if (e.type == KeyPress)
            {
                KeySym sym = XKeycodeToKeysym(s_Display, e.xkey.keycode, 0);
                //printf("KeyPress:%d sym:%d\n", e.xkey.keycode, sym);

                addKeyToQueue(1, sym);
            }
            else if (e.type == KeyRelease)
            {
                KeySym sym = XKeycodeToKeysym(s_Display, e.xkey.keycode, 0);
                //printf("KeyRelease:%d sym:%d\n", e.xkey.keycode, sym);
                addKeyToQueue(0, sym);
            }
        }

        XSetForeground(s_Display, s_Gc, 0x0000FF);
        XFillRectangle(s_Display, s_Pixmap, s_Gc, 0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY);

        for (int r = 0; r < DOOMGENERIC_RESY; ++r)
        {
            for (int c = 0; c < DOOMGENERIC_RESX; ++c)
            {
                unsigned int pixel = DG_ScreenBuffer[r * DOOMGENERIC_RESX + c];
                XSetForeground(s_Display, s_Gc, pixel);
                XDrawPoint(s_Display, s_Pixmap, s_Gc, c, r);
            }
        }

        XCopyArea(s_Display, s_Pixmap, s_Window, s_Gc, 0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY,
            0, 0);

        //XFlush(s_Display);
    }

    //printf("frame\n");
}

void DG_SleepMs(uint32_t ms)
{
    usleep (ms * 1000);
}

uint32_t DG_GetTicksMs()
{
    struct timeval  tp;
    struct timezone tzp;

    gettimeofday(&tp, &tzp);

    return (tp.tv_sec * 1000) + (tp.tv_usec / 1000); /* return milliseconds */
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
	if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
	{
		//key queue is empty

		return 0;
	}
	else
	{
		unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
		s_KeyQueueReadIndex++;
		s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

		*pressed = keyData >> 8;
		*doomKey = keyData & 0xFF;

		return 1;
	}
}

void DG_SetWindowTitle(const char * title)
{
    if (s_Window)
    {
        XChangeProperty(s_Display, s_Window, XA_WM_NAME, XA_STRING, 8, PropModeReplace, title, strlen(title));
    }
}
