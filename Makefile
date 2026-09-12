CC      = gcc
CFLAGS  = -Wall -g -I./pc
LIBS    = -lpthread
SRC     = semaphore.c pc/input.c pc/display.c
TARGET  = semaphore

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGET)
