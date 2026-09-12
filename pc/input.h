#ifndef _INPUT_H
#define _INPUT_H

#include "stdint.h"

#define BUTTON_PRESSED  1
#define BUTTON_NONE     0
#define BUTTON_QUIT     2

void inputInit(void);
void inputCleanup(void);
uint8_t buttonDebounce(void);
uint8_t isQuit(void);

#endif
