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

static int FrameBufferFd = -1;
static int* FrameBuffer = 0;

static int KeyboardFd = -1;

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned int s_PositionX = 0;
static unsigned int s_PositionY = 0;

static unsigned int s_ScreenWidth = 0;
static unsigned int s_ScreenHeight = 0;

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
    case 0x67:
        key = KEY_UPARROW;
        break;
    case 0x6C:
        key = KEY_DOWNARROW;
        break;
    case 0x1D:
        key = KEY_FIRE;
        break;
    case 0x38:
        key = KEY_USE;
        break;
    case 0x36:
    case 0x2A:
        key = KEY_RSHIFT;
        break;
    case 16: key = 'q'; break;
    case 17: key = 'w'; break;
    case 18: key = 'e'; break;
    case 19: key = 'r'; break;
    case 20: key = 't'; break;
    case 21: key = 'y'; break;
    case 22: key = 'u'; break;
    case 23: key = 'i'; break;
    case 24: key = 'o'; break;
    case 25: key = 'p'; break;
    case 30: key = 'a'; break;
    case 31: key = 's'; break;
    case 32: key = 'd'; break;
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
        printf("Getting screen width...");
        s_ScreenWidth = 640;
        printf("%d\n", s_ScreenWidth);

        printf("Getting screen height...");
        s_ScreenHeight = 480;
        printf("%d\n", s_ScreenHeight);

        if (0 == s_ScreenWidth || 0 == s_ScreenHeight)
        {
            printf("Unable to obtain screen info!");
            exit(1);
        }

        FrameBuffer = mmap(NULL, s_ScreenWidth * s_ScreenHeight * 4, PROT_READ | PROT_WRITE, 0, FrameBufferFd, 0);

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

    enableRawMode();
}

struct event {
    uint16_t typ;
    uint16_t code;
    int32_t val;
};

static void handleKeyInput()
{
    fd_set r;
    struct timeval tm;
    tm.tv_sec = 0;
    tm.tv_usec = 0;

    while (1) {
        FD_ZERO(&r);
        FD_SET(KeyboardFd, &r);
        int res = select(1, &r, 0, 0, &tm);

        if (res > 0) {
            struct event evt;
            read(KeyboardFd, &evt, sizeof(struct event));

            if (evt.val == 2) {
                // ignore repeat key events
                continue;
            }

            addKeyToQueue((evt.val == 1) ? 0 : 1, evt.code);
        } else {
            break;
        }
    }
}

void DG_DrawFrame()
{
    handleKeyInput();

    if (FrameBuffer)
    {
        for (int i = 0; i < DOOMGENERIC_RESY; ++i)
        {
            memcpy(FrameBuffer + s_PositionX + (i + s_PositionY) * s_ScreenWidth, DG_ScreenBuffer + i * DOOMGENERIC_RESX, DOOMGENERIC_RESX * 4);
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

void DG_SetWindowTitle(const char * title)
{
}
