/*
 * PC port: Keyboard input detection using select() and termios.
 * Based on "Programming Embedded Systems, 2nd Edition" (O'Reilly, 2007).
 * Original hardware-dependent code by Anthony Massa and Michael Barr.
 * PC port Copyright (c) 2026 Ethan Lin. Licensed under CC BY-NC 4.0.
 */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <fcntl.h>
#include <termios.h>
#include "stdint.h"
#include "input.h"

static volatile sig_atomic_t gQuitFlag = 0;
static struct termios gOrigTermios;

static void signalHandler(int sig)
{
    gQuitFlag = 1;
}

void inputInit(void)
{
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    tcgetattr(STDIN_FILENO, &gOrigTermios);

    struct termios raw = gOrigTermios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

void inputCleanup(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &gOrigTermios);
}

uint8_t isQuit(void)
{
    return gQuitFlag;
}

uint8_t buttonDebounce(void)
{
    struct timeval tv;
    fd_set fds;

    if (gQuitFlag)
        return BUTTON_QUIT;

    tv.tv_sec = 0;
    tv.tv_usec = 100000;

    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0)
    {
        char ch;
        ssize_t n = read(STDIN_FILENO, &ch, 1);
        if (n <= 0)
        {
            gQuitFlag = 1;
            return BUTTON_QUIT;
        }
        if (ch == 'q' || ch == 'Q')
        {
            gQuitFlag = 1;
            return BUTTON_QUIT;
        }
        return BUTTON_PRESSED;
    }

    return BUTTON_NONE;
}
