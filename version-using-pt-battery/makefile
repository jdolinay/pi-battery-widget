LIBS = `pkg-config --libs gtk+-3.0`

CFLAGS = `pkg-config --cflags gtk+-3.0`

all: pi-top-battery-widget1

pi-top-battery-widget1: pi-top-battery-widget1.c
	gcc -o pi-top-battery-widget1 pi-top-battery-widget1.c $(LIBS) $(CFLAGS)

