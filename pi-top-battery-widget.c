/*

 * pi-top-battery-widget.c
 * 
 * display pi-top battery status
 * uses /usr/bin/pt-battery to get battery charge information
 *
 * Copyright 2016, 2017  rricharz 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * Limitations:
 * ************
 *
 * Uses deprecated functions
 *   gtk_status_icon_new_from_pixbuf
 *   gtk_status_icon_set_from_pixbuf
 * 
 * Must be installed at ~/bin, loads battery_icon.png from there
 * 
 */



#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <cairo.h>
#define GDK_DISABLE_DEPRECATION_WARNINGS
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define GRAY_LEVEL      0.93	// Background of % charge display
#define REDLEVEL        10		// below this level battery charge display is red
#define INTERVAL		5000	// msec between two updates

cairo_surface_t *surface;
gint width;
gint height;

GtkWidget *MainWindow;

guint global_timeout_ref;
int first;
GtkStatusIcon* statusIcon;
FILE *battskript;

// The following function is called every INTERVAL msec
static gboolean timer_event(GtkWidget *widget)
{
	cairo_t *cr;
	GdkPixbuf *new_pixbuf;
	int w;
	char str[255];
	
	int capacity;
	char *sstatus;
	
	int chargingState;
	char battdata[2048];
	
	// stop timer in case of tc_loop taking too long
	
	g_source_remove(global_timeout_ref);
	
	// run /usr/bin/pt-battery and extract current charge and state from output
	
	battskript = popen("/usr/bin/pt-battery","r");
	if (battskript == NULL) {
		printf("Failed to run pt-battery\n");
		exit (1);
	}
	
	chargingState = -1;
	capacity = -1;

    while (fgets(battdata, 2047, battskript) != NULL) {
		sscanf(battdata,"Charging State: %d", &chargingState);
		sscanf(battdata,"Capacity:%d", &capacity);
	}
		
	// printf("Charging State: %d, ", chargingState);
	// printf("Capacity: %d, ", capacity);
    
	if ((capacity > 100) || (capacity < 0))
		capacity = -1;              // capacity out of limits

	pclose(battskript);
	
	sstatus = "unknown";
	if (chargingState == 0)
		sstatus = "discharging";
	else if (chargingState == 1)
		sstatus = "charging";
		
	// create a drawing surface
		
	cr = cairo_create (surface);
	
	// fill the battery symbol wth a colored bar
	
	if (capacity < 0)         // capacity out of limits
	  w = 0;
	else
	  w = (99 * capacity) / 400;
	if (strcmp(sstatus,"charging") == 0)
		cairo_set_source_rgb (cr, 1, 1, 0);
	else if (capacity <= REDLEVEL)
		cairo_set_source_rgb (cr, 1, 0, 0);
	else if (strcmp(sstatus,"external power") == 0)  // currently not used
	    cairo_set_source_rgb (cr, 0.5, 0.5, 0.7);
	else
		cairo_set_source_rgb (cr, 0, 1, 0);
	cairo_rectangle (cr, 5, 4, w, 12);
	cairo_fill (cr);
	if (w < 23) {
		cairo_set_source_rgb (cr, 1, 1, 1);
		cairo_rectangle (cr, 5 + w, 4, 24 - w, 12);
		cairo_fill (cr);
	}
	
	// display the capacity figure
	
	cairo_set_source_rgb (cr, GRAY_LEVEL, GRAY_LEVEL, GRAY_LEVEL);
	cairo_rectangle (cr, 0, 20, 35, 15);
	cairo_fill (cr);  
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_select_font_face(cr, "Dosis", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 12);
	if (capacity >= 0) {
	  int x = 4;
	  if (capacity < 10)
	    x += 4;
	  else if (capacity > 99)
	    x -= 4;
	  cairo_move_to(cr, x, 33);
	  sprintf(str,"%2d%%",capacity);
	  cairo_show_text(cr, str);
	}
		
	// Create a new pixbuf from the modified surface and display icon
	
	new_pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, width, height);
	
	if (first) {
		statusIcon= gtk_status_icon_new_from_pixbuf(new_pixbuf);
		first = FALSE;
	}
	else {
		gtk_status_icon_set_from_pixbuf(statusIcon,new_pixbuf);
	}
	    
	g_object_unref(new_pixbuf);
	cairo_destroy (cr);
	
	// restart timer
			
	global_timeout_ref = g_timeout_add(INTERVAL, (GSourceFunc) timer_event, (gpointer) MainWindow);
	
	return TRUE;
}

// This function is called once at startup to initialize everything

int main(int argc, char *argv[])
{
	GdkPixbuf *pixbuf, *new_pixbuf;
	cairo_t *cr;
	cairo_format_t format;
	
	first = TRUE;
	
	gtk_init(&argc, &argv);
	
	// check whether pt-battery can be executed
	
	battskript = popen("/usr/bin/pt-battery","r");
	if (battskript == NULL) {
		printf("Failed to run /usr/bin/pt-battery\n");
		exit (1);
	}
	else
		pclose(battskript);

	// create the drawing surface and fill with icon
	
	const char *homedir = getpwuid(getuid())->pw_dir;	
	char s[255]; 
	strcpy(s, homedir);
	strcat(s, "/bin/battery_icon.png" );
	// printf("s = %s\n",s);
	pixbuf = gdk_pixbuf_new_from_file (s, NULL);
	if (pixbuf == NULL) {
		printf("Cannot load icon (%s)\n", s);
		return 1;
	}
	format = (gdk_pixbuf_get_has_alpha (pixbuf)) ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	surface = cairo_image_surface_create (format, width, height);
	g_assert (surface != NULL);
	
	// Draw icon onto the surface
	
	cr = cairo_create (surface);     
	gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
	cairo_paint (cr);
	cairo_destroy (cr);
	
	// Add timer event
	// Register the timer and set time in mS.
	// The timer_event() function is called repeatedly.
	 
	global_timeout_ref = g_timeout_add(INTERVAL, (GSourceFunc) timer_event, (gpointer) MainWindow);

	// Call the timer function because we don't want to wait for the first time period triggered call

	timer_event(MainWindow);
	
	gtk_main();
	
	return 0;
}
