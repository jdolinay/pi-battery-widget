LIBS = `pkg-config --libs gtk+-3.0`
CFLAGS = `pkg-config --cflags gtk+-3.0`

all: pi-battery-widget

clean: 
	rm pi-battery-widget

pi-battery-widget: pi-battery-widget.c
	gcc -o pi-battery-widget pi-battery-widget.c $(LIBS) $(CFLAGS) -lwiringPi -std=gnu99 -lrt -lm

