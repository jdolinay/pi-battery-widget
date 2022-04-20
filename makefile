LIBS = `pkg-config --libs gtk+-3.0`

CFLAGS = `pkg-config --cflags gtk+-3.0`

all: pi-battery-widget

clean: 
	rm pi-battery-widget

pi-battery-widget: pi-battery-widget.cc
	g++ -o pi-battery-widget pi-battery-widget.cc ina219.cc $(LIBS) $(CFLAGS) -lwiringPi -lrt -lm

