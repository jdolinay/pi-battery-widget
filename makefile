LIBS = `pkg-config --libs gtk+-3.0`

CFLAGS = `pkg-config --cflags gtk+-3.0`

all: pi-top-battery-widget

pi-top-battery-widget: pi-top-battery-widget.c
	gcc -o pi-top-battery-widget pi-top-battery-widget.c $(LIBS) $(CFLAGS) -lzmq

