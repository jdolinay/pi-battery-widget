/*
 * pi-battery-widget.cc
 * 
 * Raspberry pi battery status icon on task bar
 * gets information from INA219 sensor via I2C
 *
 * Copyright 2022 Jan Dolinay <j.dolinay@email.cz>
 * Copyright 2022 Pascal Herczog <hello@theredreactor.com>
 * Copyright 2018  Chin-Kai Chang <mezlxx@gmail.com>
 * Derived from https://github.com/mezl/pi-battery-widget
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
 *   gtk_status_icon_set_tooltip_text(
 * 
 * Must be installed at ~/bin, loads battery_icon.png from there
 * Must be installed at ~/bin, loads pi-battery-reader.py from there
 * Reads the ~/.config/lxpanel/LXDE-pi/panels/pi-battery-widget.conf file if exists
 * 
 * 
 */

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <math.h>

#include <cairo.h>
#define GDK_DISABLE_DEPRECATION_WARNINGS
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <wiringPiI2C.h>


#include <stdio.h>
#include <stdlib.h>
#include "ina219.h"


#define GRAY_LEVEL      0.93	// Background of % charge display
#define REDLEVEL        10		// below this level battery charge display is red
#define INTERVAL		8000	// msec between two updates

#define MAKELOG         0	// log file batteryLog in home directory (0 = no log file)
#define SHUTDOWN		0	// do shutdown (0 = no shutdown, just messages)

#define BATTERY_VMAX 4.2
#define BATTERY_OVER 0.02	// Capacity offset while charging
#define BATTERY_UNDER 0.05	// Capacity offset while not charging
#define BATTERY_VMIN 3.1	// Safe minimum
#define BATTERY_WARN 20		// capacity %
#define BATTERY_EMTY 0		// Shutdown capacity %
#define BATTERY_SHTD 60		// Seconds to shutdown

// Reference for measured data
#define DEFAULT_CAPACITY 5000 // mAh

// Get array size
#define ROWS(arr) ((int) (sizeof (arr) / sizeof (arr)[0]))

// Set to FALSE for general use
#define DEBUG FALSE 

enum State{NO_BATT = -1, DISCHARGE = 0, CHARGE = 1, EXT_PWR = 2};
cairo_surface_t *surface;
gint width;
gint height;

GtkWidget *MainWindow;

guint global_timeout_ref;
int first;
GtkStatusIcon* statusIcon;
FILE *logFile;

// Define pop-up window handle and controls
GtkWidget *popup_window;
int popup_alarm;

int last_capacity, last_state, last_time;
double last_voltage;
double start_time; 
struct timespec resolution;

double v_one, v_two, v_three, v_average;
double a_one, a_two, a_three, a_average;
int batSize = DEFAULT_CAPACITY;
int shut_down_timer;

int iconSize;

// jd: ina219 init 
float SHUNT_OHMS = 0.1;
float MAX_EXPECTED_AMPS = 3.2;    
INA219 g_i(SHUNT_OHMS, MAX_EXPECTED_AMPS);
bool g_inaConfigured = false;	// true if INA219 sensor was found, set in main()


double timer_me(int reset)
{
	double current_time, elapsed_time;

	clock_gettime(CLOCK_MONOTONIC, &resolution);
	current_time = (resolution.tv_sec) ;

	elapsed_time = current_time - start_time;
	
	if(reset) start_time = (resolution.tv_sec);

	return elapsed_time;

}

void printLogEntry(int capacity, const char* state, char* timeStr, double voltage) {
	time_t rawtime;
	struct tm *timeinfo;
	char timeString[80];

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(timeString, 80, "%D %R:%S", timeinfo);
	fprintf(logFile,"%s", timeString);	

	fprintf(logFile, ", Capacity: %3d%%", capacity);

	fprintf(logFile, ", T-Tip: %s, %s", state, timeStr);
	fprintf(logFile, ", I2C Voltage: %2.3fV\n",voltage);
	fflush(logFile);
}

// Battery Low Warning pop-up
void show_warning(GtkWidget *widget, gpointer window, const char* msg_lbl, const char* msg_txt) {
    
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new(GTK_WINDOW(window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_OK,			
			msg_txt);
	//"Battery LOW!"
	gtk_window_set_title(GTK_WINDOW(dialog), msg_lbl);

	// Run without blocking, response closes
	g_signal_connect_swapped (dialog,
							"response",
							G_CALLBACK (gtk_widget_destroy),
							dialog);
	gtk_widget_show_all (dialog);
}

// Define battery specs, scaled by capacity
float charge_model[6][3] = {
	{2.400, 3.000, 600.0f * batSize/DEFAULT_CAPACITY},
	{3.000, 3.352, 550.0f * batSize/DEFAULT_CAPACITY},
	{3.352, 3.464, 620.0f * batSize/DEFAULT_CAPACITY},
	{3.464, 3.548, 1270.0f * batSize/DEFAULT_CAPACITY},
	{3.548, 4.132, 16840.0f * batSize/DEFAULT_CAPACITY},
	{4.132, 4.212, 8590.0f * batSize/DEFAULT_CAPACITY}
};
float discharge_model[5][4] = {
	{2.9, 2.992, 520.0f * batSize/DEFAULT_CAPACITY, 1453.1},
	{2.992, 3.324, 2450.0f * batSize/DEFAULT_CAPACITY, 1330.2},
	{3.324, 3.868, 10610.0f * batSize/DEFAULT_CAPACITY, 1152.4},
	{3.868, 4.05, 4160.0f * batSize/DEFAULT_CAPACITY, 1057.8},
	{4.05, 4.2 + BATTERY_OVER, 120.0f * batSize/DEFAULT_CAPACITY, 950.0}
};
		
// The following function is called every INTERVAL msec
static gboolean timer_event(GtkWidget *widget)
{
	cairo_t *cr;
	GdkPixbuf *new_pixbuf;
	int w;
	char str[255];
	char timeStr[255];

	int capacity, time;
	double voltage;
	float current;
	
	char const *sstatus;
	int response;

	int chargingState;
	char battdata[2048];

	// stop timer in case of tc_loop taking too long
	g_source_remove(global_timeout_ref);

	chargingState = NO_BATT;
	capacity = -1;
	time = -1;

	response = 0;
	if ( g_inaConfigured ) {
		voltage = g_i.supply_voltage();	
		current = g_i.current();
		if(DEBUG && 1)
			printf("INA reading: voltage: %.2f, supply: %.2f, shunt: %.2f\n", g_i.voltage(), voltage, g_i.shunt_voltage());
		
	} else {
		voltage = 0.0;
		current = 0.0;
	}
	
	// Process battery data to calculate states	
	// jd: wider limist; the current is in mA
	// if ((0 < current) && (current < 10)){
	if ((-20 < current) && (current < 30)){
		// Charged, running off MAINS
		chargingState = EXT_PWR;
	}
	else if (current < 0){
		chargingState = CHARGE;
		// Charging battery, but need positive for sums
		current = -current;
	} else {
		chargingState = DISCHARGE;
	}
	
	// Check valid data
	if ((current < 10000) && (voltage != 0)){
		response = 1;
	} else {
		current = 0;
		voltage = 0;
		chargingState = NO_BATT;		
	}
	
	// Perform averaging to avoid spikes
	if(last_voltage < 0){
		last_voltage = voltage;
		v_one = voltage;
		v_two = voltage;
		v_three = voltage;
		v_average = voltage;
		a_one = current;
		a_two = current;
		a_three = current;
		a_average = current;
	}else{
		v_average = (voltage + v_one + v_two + v_three) / 4;
		v_three = v_two;
		v_two = v_one;
		v_one = voltage;
		a_average = (current + a_one + a_two + a_three) / 4;
		a_three = a_two;
		a_two = a_one;
		a_one = current;
	}
	
	if (chargingState == CHARGE){
		capacity = (v_average - BATTERY_VMIN) / (BATTERY_VMAX + BATTERY_OVER - BATTERY_VMIN) * 100;
	}
	else {
		// Use for Full, Discharge and No Bat
		capacity = (v_average - BATTERY_VMIN) / (BATTERY_VMAX - BATTERY_UNDER - BATTERY_VMIN) * 100;
	}
	
	if (voltage > 4.25){
		chargingState = NO_BATT;
		current = 0;
		capacity = 0;
	}
	
	// If battery voltage allowed to go below VMIN, stay at 0%
	if (capacity < 0) 
		capacity = 0;
	if (capacity > 100) 
		capacity = 100;


	if(chargingState == NO_BATT){
		// no battery
		if(DEBUG && 1) printf("No BATT - Mains:");
		
	} else if(chargingState == DISCHARGE) {
		
		// calculate remaining discharge time in minutes for tooltip, using averaged voltage/current
		
		float drain_time = 0.0;
		float power_ratio = 1.0;
		
		// First valid row is always partial
		for (int i = ROWS(discharge_model) - 1; i >= 0; --i) {
			if(v_average >= discharge_model[i][1]){
				// BATTERY_OVER ensures i+1 < ROWS - 1
				power_ratio *= discharge_model[i][3] / discharge_model[i+1][3];
				drain_time += discharge_model[i][2] * discharge_model[i][3] / (a_average * power_ratio);
			} else if (v_average < discharge_model[i][0]){
				// don't use this curve
				// printf("Skip row %d\n", i);
			} else{
				drain_time += ((v_average - discharge_model[i][0]) / (discharge_model[i][1] - discharge_model[i][0])) * discharge_model[i][2] * discharge_model[i][3] / a_average;
			}
		}
			
		// Displayed in minutes
		time = (int)drain_time/60;
		if(DEBUG && 1) printf("Discharging - Time Left: %d min, (av)Vd: %.3f, (av)mA: %.2f\n",time,v_average,a_average);


	} else if(chargingState == CHARGE) {

		// calculate remaining charging time in minutes for tooltip, using averaged voltage
		
		float charge_time = 0.0;
		
		// Use size of array
		for (int i = 0; i < ROWS(charge_model); ++i) {
			if(v_average >= charge_model[i][1]){
				// don't use this curve
				// printf("Skipping");
			} else if (v_average <= charge_model[i][0]){
				// Add this charge curve
				charge_time += charge_model[i][2];
			} else{
				charge_time += ((charge_model[i][1] - v_average) / (charge_model[i][1] - charge_model[i][0])) * charge_model[i][2];
			}
		}
			
		// Displayed in minutes (+1 as clearly not yet finished if <1)
		time = (int)charge_time/60 + 1;
		if(DEBUG && 1) printf("Charging - Time Left: %d min, (av)Vd: %.3f, (av)mA: %.2f\n",time,v_average,a_average);

	} else {
		if(chargingState == EXT_PWR){
			if(DEBUG && 1) printf("EXT_PWR - Battery FULL at %.3f\n", voltage);
		}

		time = last_time;
	}


	if(DEBUG && 1) printf("pi-battery-widget: V=%.3f, mA=%.2f, capacity=%d%%, status=%d\n", voltage, current, capacity, chargingState);

	
	// Only if external power, same state and voltage as last then no update needed
	if ((chargingState == last_state) && (chargingState == EXT_PWR) && (voltage == last_voltage)) {
		// restart timer and return without updating icon
		global_timeout_ref = g_timeout_add(INTERVAL, (GSourceFunc) timer_event, (gpointer) MainWindow);	
		if(DEBUG && 1) printf("state does not change, no update\n");
		return TRUE;		
	}


	sstatus = "no battery";
	if (chargingState == DISCHARGE)
		sstatus = "Discharging";
	else if (chargingState == CHARGE)
		sstatus = "Charging";
	else if (chargingState == EXT_PWR)
		sstatus = "External power";

	// prepare tooltip
	char printTime[32];
	//sprintf(timeStr, "%s", sstatus);
	//if (strcmp(sstatus,"charging") == 0) {
	if ( chargingState == CHARGE ) {
		if ((time > 0) && (time < 1000)) {
			if ( time > 90 )
				sprintf(printTime, "%.1f hours", (float)time / 60.0f);
			else
				sprintf(printTime, "%d minutes", time);
				
			sprintf(timeStr, "(%d%%) %s\nCharging time: %s\nV=%.2f, mA=%.0f", 
						capacity, sstatus, printTime, v_average, a_average);			
		}
	}
	else if ( chargingState == DISCHARGE ) {	//(strcmp(sstatus,"discharging") == 0) {
		
		if ((time > 0) && (time < 1000)) {
			if ( time > 90 )
				sprintf(printTime, "%.1f hours", (float)time / 60.0f);
			else
				sprintf(printTime, "%d minutes", time);
							
				sprintf(timeStr, "(%d%%) %s,\nTime remaining: %s\nV=%.2f, mA=%.0f", 				
						capacity, sstatus, printTime, v_average, a_average);			
		}
		
	} else if ( chargingState == EXT_PWR ) {
		
		sprintf(timeStr, "Fully Charged\nVoltage = %.2f", v_average);
		
	} else if ( chargingState == NO_BATT ) {
		
		sprintf(timeStr, "No battery detected");
		
	} else {		
			// should never happen
			sprintf(timeStr, "Internal error - unknown chargingState %d", (int)chargingState);		
	}

	// update log
	printLogEntry(capacity, sstatus, timeStr, voltage);

	// create a drawing surface
	cr = cairo_create (surface);

	// scale surface, if necessary

	if (width != iconSize) {
		double scaleFactor = (double) iconSize / (double) width;
		if (iconSize >= 39) {		
			double scaleFactor = (double) (iconSize - 3) / (double) width;
			cairo_translate(cr, 0.0, 3.0);
		}
		else {
			double scaleFactor = (double) iconSize / (double) width;
		}
		cairo_scale(cr, scaleFactor,scaleFactor);
	}

	// fill the battery symbol with a colored bar

	if (capacity < 0)         // capacity out of limits
		w = 0;
	else
		w = (99 * capacity) / 400;
	if (strcmp(sstatus,"charging") == 0)
		cairo_set_source_rgb (cr, 1, 1, 0);//Yellow
	else if (capacity <= REDLEVEL)
		cairo_set_source_rgb (cr, 1, 0, 0); //RED
	else if (strcmp(sstatus,"externally powered") == 0)
		cairo_set_source_rgb (cr, 0, 1, 0);//Green
	else if (strcmp(sstatus,"no battery") == 0)
		cairo_set_source_rgb (cr, 0, 0, 0);//Black
	else
		cairo_set_source_rgb (cr, 0.5, 0.5, 0.7);//GRAY
	cairo_rectangle (cr, 5, 4, w, 12);
	cairo_fill (cr);
	if (w < 23) {
		cairo_set_source_rgb (cr, 1, 1, 1);//White
		cairo_rectangle (cr, 5 + w, 4, 24 - w, 12);
		cairo_fill (cr);
	}

	// display the capacity figure
	cairo_set_source_rgb (cr, GRAY_LEVEL, GRAY_LEVEL, GRAY_LEVEL);
	cairo_rectangle (cr, 0, 20, 35, 15);
	cairo_fill (cr);  
	cairo_set_source_rgb (cr, 0, 0, 0);//BLACK
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
	new_pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, iconSize, iconSize);

	if (first) {
		statusIcon= gtk_status_icon_new_from_pixbuf(new_pixbuf);
		first = FALSE;
	}
	else {
		gtk_status_icon_set_from_pixbuf(statusIcon, new_pixbuf);
	}
	gtk_status_icon_set_tooltip_text(statusIcon, timeStr);

	g_object_unref(new_pixbuf);
	cairo_destroy (cr);
	
	// Control forced shutdown, print critical msgs
	if ((capacity <= BATTERY_EMTY) && (chargingState == DISCHARGE)){
		printf("Battery Capacity at 0%, shutting down in %d seconds!\n", shut_down_timer);
		if (shut_down_timer == BATTERY_SHTD) {
			popup_alarm = TRUE;
			show_warning(popup_window, MainWindow, "Battery EMPTY!", "System Shutting Down in 60s!!");
		}
		if (shut_down_timer <= 0) {
			printf("Battery EMPTY, forcing system shutdown NOW!\n");
			fprintf(logFile,"Battery EMPTY, forcing system shutdown NOW!\n");
			fclose(logFile);
#if SHUTDOWN
			system("sudo shutdown now");
#endif
			gtk_main_quit();
		}
		shut_down_timer -= INTERVAL/1000;
	}
	
	// Display Battery Low if popup not already active
	if ((capacity <= BATTERY_WARN) && (chargingState == DISCHARGE) && (popup_alarm == FALSE)) {
		if(DEBUG && 1) printf("Battery LOW, please enable exteral power\n");
		popup_alarm = TRUE;
		show_warning(popup_window, MainWindow, "Battery LOW!", "Connect external power supply");
	}
	
	// Reset alarm status if charging
	if ((chargingState == CHARGE) && (popup_alarm == TRUE)) {
		popup_alarm = FALSE;
		if (shut_down_timer <= BATTERY_SHTD) {
			if(DEBUG && 1) printf("Charger detected, cancelling shutdown timer\n");
			fprintf(logFile,"Charger detected, cancelling shutdown timer\n");
			shut_down_timer = BATTERY_SHTD;
		}
	}

	last_capacity = capacity;
	last_voltage = voltage;
	last_state = chargingState;
	last_time = time;

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
	
	
	// Setup Alarm controls
	popup_alarm = FALSE;
	shut_down_timer = BATTERY_SHTD;

	// Track deltas between calls
	first = TRUE;
	last_capacity = -1;
	last_voltage = -1.0;
	last_state = -1;
	last_time = -1;

	gtk_init(&argc, &argv);

	// open log file
	const char *homedir = getpwuid(getuid())->pw_dir;	
	char s[255];
	if (MAKELOG) {
		strcpy(s, homedir);
		strcat(s, "/pi-battery-widget_batteryLog.txt" );
		// Avoid logfile getting too big with daily use
		logFile = fopen(s,"w");
		fprintf(logFile, "Starting pi-battery-widget\n");
	}
	else
		logFile = stdout;
		
	// jd: init INA lib	
    g_inaConfigured = g_i.configure(RANGE_16V, GAIN_8_320MV, ADC_12BIT, ADC_12BIT);
    if (!g_inaConfigured ) {
		g_inaConfigured = false;
		printf("INA219 Sensor not found, input not available.\n");
		fprintf(logFile,"INA219 Sensor not found, input not available.\n");
	}

	// get LXDE panel icon size
	iconSize = -1;
	strcpy(s, homedir);
	strcat(s, "/.config/lxpanel/LXDE-pi/panels/pi-battery-widget.conf");
	FILE* lxpanel = fopen(s, "r");
	if (lxpanel == NULL) {
		printf("Failed to open LXDE panel config file %s\n", s);
		fprintf(logFile,"Failed to open LXDE panel config file %s\n", s);
	}
	else {
		char lxpaneldata[2048];
		while ((fgets(lxpaneldata, 2047, lxpanel) != NULL)) {
			sscanf(lxpaneldata,"iconsize=%d", &iconSize);
			sscanf(lxpaneldata,"capacity=%d", &batSize);
		}
		fclose(lxpanel);
	}
	if (iconSize == -1)    // default
		iconSize = 48;  //36;
	
	// Check for sensible capacity, cannot be less than 1000mAh
	if ((batSize < 1000) || (batSize > 10000)){
		printf("Failed to obtain valid battery capacity: %dmAh, changing to default %d\n", batSize, DEFAULT_CAPACITY);
		fprintf(logFile,"Failed to obtain valid battery capacity: %dmAh, changing to default %d\n", batSize, DEFAULT_CAPACITY);
		batSize = DEFAULT_CAPACITY;
	}
	fprintf(logFile, "Config Data: Icon Size=%d, Capacity=%dmAh\n", iconSize, batSize);

	// create the drawing surface and fill with icon

	strcpy(s, homedir);
	strcat(s, "/bin/battery_icon.png" );
	// printf("s = %s\n",s);
	pixbuf = gdk_pixbuf_new_from_file (s, NULL);
	if (pixbuf == NULL) {
		printf("Cannot load icon (%s)\n", s);
		fprintf(logFile,"Cannot load icon (%s)\n", s);
		return 1;
	}
	format = (gdk_pixbuf_get_has_alpha (pixbuf)) ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
	height = gdk_pixbuf_get_height (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	surface = cairo_image_surface_create (format, iconSize, iconSize);
	g_assert (surface != NULL);

	cr = cairo_create (surface);


	// scale surface, if necessary

	if (width != iconSize) {
		double scaleFactor = (double) iconSize / (double) width;
		if (iconSize >= 39) {
		//if (iconSize >= 48) {
			double scaleFactor = (double) (iconSize - 3) / (double) width;
			cairo_translate(cr, 0.0, 3.0);
		}
		else {
			double scaleFactor = (double) iconSize / (double) width;
		}
		cairo_scale(cr, scaleFactor,scaleFactor);
	}

	// Draw icon onto the surface

	gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
	cairo_paint (cr);
	cairo_destroy (cr);

	// Add timer event
	// Register the timer and set time in mS.
	// The timer_event() function is called repeatedly.
	
	// jd: if sensor was not found, do not set the timer?
	// but then no icon is shown. I prefer to have the icon with "no battery"
	// so let the timer run. The handler will not attempt to read from INA219
	// anyway as g_inaConfigured is false.	
	global_timeout_ref = g_timeout_add(INTERVAL, (GSourceFunc) timer_event, (gpointer) MainWindow);

	// Call the timer function because we don't want to wait for the first time period triggered call
	clock_gettime(CLOCK_MONOTONIC, &resolution);
	start_time = (resolution.tv_sec); 
	timer_event(MainWindow);		

	gtk_main();

	return 0;
}
