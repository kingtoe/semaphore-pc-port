/*
 * PC port: LED output simulation using printf.
 * Based on "Programming Embedded Systems, 2nd Edition" (O'Reilly, 2007).
 * Original hardware-dependent code by Anthony Massa and Michael Barr.
 * PC port Copyright (c) 2026 Ethan Lin. Licensed under CC BY-NC 4.0.
 */

#include <stdio.h>
#include "display.h"

static int ledState = 0;

void ledInit(void)
{
    ledState = 0;
    printf("[LED] 初始化完成\n");
    printf("[LED] 状态: OFF\n");
}

void ledToggle(void)
{
    ledState = !ledState;
    printf("[LED] 状态: %s\n", ledState ? "ON " : "OFF");
}
