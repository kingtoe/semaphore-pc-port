/*
 * POSIX semaphore producer-consumer example (PC port).
 * Based on "Programming Embedded Systems, 2nd Edition" (O'Reilly, 2007).
 * Original code by Anthony Massa and Michael Barr. All rights reserved.
 * Original license: non-commercial use, modification, and distribution
 * permitted with this notice retained. See source headers for details.
 * PC port Copyright (c) 2026 Ethan Lin. Licensed under CC BY-NC 4.0.
 */

#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#include "pc/input.h"
#include "pc/display.h"


pthread_t consumerTaskObj;
pthread_t producerTaskObj;

sem_t semButton;


void producerTask(void *param)
{
    uint8_t result;

    while (1)
    {
        usleep(100000);

        result = buttonDebounce();

        if (result == BUTTON_QUIT)
            break;

        if (result == BUTTON_PRESSED)
            sem_post(&semButton);
    }
}


void consumerTask(void *param)
{
    struct timespec ts;

    while (1)
    {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 200000000;
        if (ts.tv_nsec >= 1000000000)
        {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }

        if (sem_timedwait(&semButton, &ts) == 0)
        {
            printf("[Consumer] 按键事件触发，翻转 LED\n");
            ledToggle();
        }

        if (isQuit())
            break;
    }
}


int main(void)
{
    inputInit();
    ledInit();

    sem_init(&semButton, 0, 0);

    pthread_create(&producerTaskObj, NULL, (void *)producerTask, NULL);
    pthread_create(&consumerTaskObj, NULL, (void *)consumerTask, NULL);

    printf("PC 移植版 Semaphore 示例 - 按任意键触发 LED 翻转，Ctrl+C 或 q 退出\n");

    pthread_join(producerTaskObj, NULL);
    pthread_join(consumerTaskObj, NULL);

    sem_destroy(&semButton);
    inputCleanup();

    printf("程序退出\n");

    return 0;
}
