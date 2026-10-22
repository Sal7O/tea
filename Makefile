CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=c99 -ggdb `pkg-config --cflags sdl2`
LIBS=-lm `pkg-config --libs sdl2`

tea: src/main.c
	$(CC) $(CFLAGS) $(LIBS) -o tea src/main.c