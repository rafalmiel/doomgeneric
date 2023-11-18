//doomgeneric for soso os

#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <stdlib.h>
#include <sys/types.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <termios.h>

#include <cykusz/syscall.h>

static uint64_t GFBINFO = 0x3415;

struct fb_info {
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint64_t bpp;
};

struct event {
    struct timeval tim;
    uint16_t typ;
    uint16_t code;
    int32_t val;
};

struct mouse_event {
    int buttons;
    int rel_x;
    int rel_y;
    int got_data;
};

static int FrameBufferFd = -1;
static int* FrameBuffer = 0;

static int KeyboardFd = -1;
static int MouseFd = -1;

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static struct mouse_event s_mouse;

static unsigned int s_PositionX = 0;
static unsigned int s_PositionY = 0;

static unsigned int s_ScreenWidth = 0;
static unsigned int s_ScreenHeight = 0;
static unsigned int s_ScreenPitch = 0;

static unsigned char convertToDoomKey(unsigned char scancode)
{
    unsigned char key = 0;

    switch (scancode)
    {
    case 0x1C:
        key = KEY_ENTER;
        break;
    case 0x01:
        key = KEY_ESCAPE;
        break;
    case 0x69:
        key = KEY_LEFTARROW;
        break;
    case 0x6A:
        key = KEY_RIGHTARROW;
        break;
    case 17: // w
        key = KEY_UPARROW;
        break;
    case 31: // s
        key = KEY_DOWNARROW;
        break;
    case 0x1D:
        key = KEY_FIRE;
        break;
    case 0x39:
        key = KEY_USE;
        break;
    case 30: // a
        key = KEY_STRAFE_L;
        break;
    case 32: // d
        key = KEY_STRAFE_R;
        break;
    case 0x36:
    case 0x2A:
        key = KEY_RSHIFT;
        break;
    case 16: key = 'q'; break;
    case 18: key = 'e'; break;
    case 19: key = 'r'; break;
    case 20: key = 't'; break;
    case 21: key = 'y'; break;
    case 22: key = 'u'; break;
    case 23: key = 'i'; break;
    case 24: key = 'o'; break;
    case 25: key = 'p'; break;
    case 33: key = 'f'; break;
    case 34: key = 'g'; break;
    case 35: key = 'h'; break;
    case 36: key = 'j'; break;
    case 37: key = 'k'; break;
    case 38: key = 'l'; break;
    case 44: key = 'z'; break;
    case 45: key = 'x'; break;
    case 46: key = 'c'; break;
    case 47: key = 'v'; break;
    case 48: key = 'b'; break;
    case 49: key = 'n'; break;
    case 50: key = 'm'; break;
    default:
        break;
    }

    return key;
}

static void addKeyToQueue(int pressed, unsigned char keyCode)
{
    unsigned char key = convertToDoomKey(keyCode);

    unsigned short keyData = (pressed << 8) | key;

    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex++;
    s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}


struct termios orig_termios;

void disableRawMode()
{
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode()
{
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void DG_Init()
{
    FrameBufferFd = open("/dev/fb", 0);

    if (FrameBufferFd >= 0)
    {
        struct fb_info finfo;
        ioctl(FrameBufferFd, GFBINFO, &finfo);
        printf("Getting screen width...");
        s_ScreenWidth = finfo.width;
        printf("%d\n", s_ScreenWidth);

        printf("Getting screen height...");
        s_ScreenHeight = finfo.height;
        printf("%d\n", s_ScreenHeight);

        printf("Getting screen pitch...");
        s_ScreenPitch = finfo.pitch;
        printf("%d\n", s_ScreenPitch);

        if (0 == s_ScreenWidth || 0 == s_ScreenHeight)
        {
            printf("Unable to obtain screen info!");
            exit(1);
        }

        FrameBuffer = mmap(NULL, s_ScreenPitch * s_ScreenHeight, PROT_READ | PROT_WRITE, 0, FrameBufferFd, 0);

        if (FrameBuffer != (int*)-1)
        {
            printf("FrameBuffer mmap success\n");
        }
        else
        {
            printf("FrameBuffermmap failed\n");
        }
    }
    else
    {
        printf("Opening FrameBuffer device failed!\n");
    }

    KeyboardFd = open("/dev/kbd", O_RDONLY);
    MouseFd = open("/dev/mouse", O_RDONLY);

    enableRawMode();
}

static void handleInput()
{
    fd_set r;
    struct timeval tm;
    tm.tv_sec = 0;
    tm.tv_usec = 0;

    while (1) {
        FD_ZERO(&r);
        FD_SET(KeyboardFd, &r);
        FD_SET(MouseFd, &r);
        int res = select(2, &r, 0, 0, &tm);

        if (res > 0) {
            if (FD_ISSET(KeyboardFd, &r)) {
                struct event evt;
                read(KeyboardFd, &evt, sizeof(struct event));

                if (evt.val == 2) {
                    // ignore repeat key events
                    continue;
                }

                addKeyToQueue((evt.val == 1) ? 0 : 1, evt.code);
            }

            if (FD_ISSET(MouseFd, &r)) {
                struct event evt;
                read(MouseFd, &evt, sizeof(struct event));

                s_mouse.got_data = 1;

                switch (evt.typ) {
                    case 1: {
                        int idx;
                        switch (evt.code) {
                            case 0x110: { // mouse left
                                idx = 0;
                            }
                            break;
                            case 0x111: { // mouse right
                                idx = 1;
                            }
                            break;
                            case 0x112: { // mouse middle
                                idx = 2;
                            }
                        }
                        if (evt.val) {
                            s_mouse.buttons |= (1 << idx);
                        } else {
                            s_mouse.buttons &= ~(1 << idx);
                        }
                    }
                    break;
                    case 2: {
                        switch (evt.code) {
                            case 0: { //rel x
                                s_mouse.rel_x += evt.val;
                            }
                            break;
                            case 1: { //rel y
                                s_mouse.rel_y += evt.val;
                            }
                            break;
                        }
                    }
                    break;
                }
            }
        } else {
            break;
        }
    }
}

void DG_DrawFrame()
{
    handleInput();

    if (FrameBuffer)
    {
        for (int i = 0; i < DOOMGENERIC_RESY; ++i)
        {
            memcpy(FrameBuffer + s_PositionX + (i + s_PositionY) * s_ScreenPitch / 4, DG_ScreenBuffer + i * DOOMGENERIC_RESX, DOOMGENERIC_RESX * 4);
        }
    }
}

void DG_SleepMs(uint32_t ms)
{
    syscalln1(SYS_SLEEP, (uint64_t)ms * 1000000);
}

uint32_t DG_GetTicksMs()
{
    uint32_t ticksMs = syscalln0(SYS_TICKSNS) / 1000000;
    return ticksMs;
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

int DG_GetMouse(int* buttons, int* rel_x, int* rel_y) {
    if (s_mouse.got_data == 0) {
        return 0;
    }

    *buttons = s_mouse.buttons;
    *rel_x = s_mouse.rel_x;
    *rel_y = s_mouse.rel_y;
    //printf("mouse evt: %d %d %d\n", *buttons, *rel_x, *rel_y);

    s_mouse.got_data = 0;
    s_mouse.rel_x = 0;
    s_mouse.rel_y = 0;

    return 1;
}

void DG_SetWindowTitle(const char * title)
{
}

int main(int argc, char **argv)
{
    doomgeneric_Create(argc, argv);

    for (int i = 0; ; i++)
    {
        doomgeneric_Tick();
    }
    

    return 0;
}
