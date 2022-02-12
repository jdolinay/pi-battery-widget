/*
 * pi-battery-widget.c
 * 
 * The Red Reactor raspberry pi battery status icon on task bar
 * gets information from the I2C(another python script)
 *
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
 * Reads the ~/.config/lxpanel/LXDE-pi/panels/redreactor.conf file if exists
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


#define GRAY_LEVEL      0.93	// Background of % charge display
#define REDLEVEL        10		// below this level battery charge display is red
#define INTERVAL		5000	// msec between two updates

#define MAKELOG         1	// log file batteryLog in home directory (0 = no log file)

#define BATTERY_VMAX 4.2
#define BATTERY_OVER 0.02	// Capacity offset while charging
#define BATTERY_UNDER 0.05	// Capacity offset while not charging
#define BATTERY_VMIN 2.9	// Safe minimum
#define BATTERY_WARN 10		// capacity %
#define BATTERY_EMTY 0		// Shutdown capacity %
#define BATTERY_SHTD 30		// Seconds to shutdown

//Reference for measured data
#define DEFAULT_CAPACITY 6000 //mAh

//Get array size
#define ROWS(arr) ((int) (sizeof (arr) / sizeof (arr)[0]))

//Set to FALSE for general use
#define DEBUG FALSE 

enum State{NO_BATT = -1,DISCHARGE = 0,CHARGE = 1, EXT_PWR = 2};
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

double timer_me(int reset)
{
	double current_time, elapsed_time;

	clock_gettime(CLOCK_MONOTONIC, &resolution);
	current_time = (resolution.tv_sec) ;

	elapsed_time = current_time - start_time;
	//if(elapsed_time < 0) {
	//    elapsed_time = elapsed_time + 1000000; //in case clock_gettime wraps around
	//}
	//long ms = round(elapsed_time/ 1.0e6);

	//if(reset) start_time = resolution.tv_nsec;
	if(reset) start_time = (resolution.tv_sec);

	return elapsed_time;

}
int exec(const char* cmd,char* buffer){
	FILE *fp;

	/* Open the command for reading. */
	fp = popen(cmd, "r");
	if (fp == NULL) {
		printf("Failed to run command\n" );
		exit(1);
	}

	char s[255];
	/* Read the output a line at a time - Keep last line */
	while (fgets(s, sizeof(s)-1, fp) != NULL) {
		//printf("RAW[%s]", s);
		strcpy(buffer, s);
	}

	/* close */
	pclose(fp);

	return 0;

}

void printLogEntry(int capacity, char* state, char* timeStr, double voltage) {
	time_t rawtime;
	struct tm *timeinfo;
	char timeString[80];

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(timeString, 80, "%D %R:%S", timeinfo);
	fprintf(logFile,"%s", timeString);
	// printf("%s:  %s %d\n", timeString, s, i);

	fprintf(logFile, ", Capacity: %3d%%", capacity);

	fprintf(logFile, ", T-Tip: %s, %s", state, timeStr);
	fprintf(logFile, ", I2C Voltage: %2.3fV\n",voltage);
	fflush(logFile);
}

// Battery Low Warning pop-up
void show_warning(GtkWidget *widget, gpointer window, char* msg_lbl, char* msg_txt) {
    
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new(GTK_WINDOW(window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_OK,
			//"Enable external power supply"
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
	
	char *sstatus;
	int response;

	int chargingState;
	char battdata[2048];


	int getdata()
	{
		const char *homedir = getpwuid(getuid())->pw_dir;	
		char s[255];
		strcpy(s, homedir);
		strcat(s, "/bin/pi-battery-reader.py" );
		char buffer [256];
		float vv;
		
		// Get RedReactor data
		response = 0;
		exec (s,buffer);
		sscanf(buffer, "%f|%f",&vv, &current);
		voltage = (double)vv;
		
		// Process RedReactor data to calculate states
		if ((0 < current) && (current < 10)){
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
		if (capacity < 0) capacity = 0;
		if (capacity > 100) capacity = 100;

		// Define battery specs, scaled by capacity
		float charge_model[6][3] = {
			{2.400, 3.000, 600 * batSize/DEFAULT_CAPACITY},
			{3.000, 3.352, 550 * batSize/DEFAULT_CAPACITY},
			{3.352, 3.464, 620 * batSize/DEFAULT_CAPACITY},
			{3.464, 3.548, 1270 * batSize/DEFAULT_CAPACITY},
			{3.548, 4.132, 16840 * batSize/DEFAULT_CAPACITY},
			{4.132, 4.212, 8590 * batSize/DEFAULT_CAPACITY}
		};
		float discharge_model[5][4] = {
			{2.9, 2.992, 520 * batSize/DEFAULT_CAPACITY, 1453.1},
			{2.992, 3.324, 2450 * batSize/DEFAULT_CAPACITY, 1330.2},
			{3.324, 3.868, 10610 * batSize/DEFAULT_CAPACITY, 1152.4},
			{3.868, 4.05, 4160 * batSize/DEFAULT_CAPACITY, 1057.8},
			{4.05, 4.2 + BATTERY_OVER, 120 * batSize/DEFAULT_CAPACITY, 950.0}
		};
		
		int i;

		// enum State{NO_BATT = -1,DISCHARGE = 0,CHARGE = 1, EXT_PWR = 2};
		if(chargingState == NO_BATT){
			// chargingState = NO_BATT; //no battery
			if(DEBUG && 1) printf("No BATT - Mains:");
		} else if (chargingState == DISCHARGE){
			
			// calculate remaining discharge time in minutes for tooltip, using averaged voltage/current
			
			float drain_time = 0.0;
			float power_ratio = 1.0;
			
			// First valid row is always partial
			for (i = ROWS(discharge_model) - 1; i >= 0; --i) {
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


		}else if(chargingState == CHARGE){

			// calculate remaining charging time in minutes for tooltip, using averaged voltage
			
			float charge_time = 0.0;
			
			// Use size of array
			for (i = 0; i < ROWS(charge_model); ++i) {
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

		}else{
			if(chargingState == EXT_PWR){
				if(DEBUG && 1) printf("EXT_PWR - Battery FULL at %.3f\n", voltage);
			}

			time = last_time;
		}


		if (response == 1) {
			//printf("Raw reading [%s]",buffer);
			//printf("Got Voltage %7.5f Capacity %5d %%\n",voltage,capacity);
			return 0;
		}
		else {
			printf("No proper response received from script\n");
			return 1;
		}
	}

	// stop timer in case of tc_loop taking too long

	g_source_remove(global_timeout_ref);

	chargingState = NO_BATT;
	capacity = -1;
	time = -1;

	// run script to get information
	getdata();

	if(DEBUG && 1) printf("RedReactor: V=%.3f, mA=%.2f, capacity=%d%%, status=%d\n", voltage, current, capacity, chargingState);

	if ((capacity > 100) || (capacity < 0))
		capacity = -1;              // capacity out of limits


	// Only if external power, same state and voltage as last then no update needed
	if ((chargingState == last_state) && (chargingState == EXT_PWR) && (voltage == last_voltage)) {
		// restart timer and return without updating icon
		global_timeout_ref = g_timeout_add(INTERVAL, (GSourceFunc) timer_event, (gpointer) MainWindow);	
		if(DEBUG && 1) printf("state does not change, no update\n");
		return TRUE;		
	}

	sstatus = "no battery";
	if (chargingState == DISCHARGE)
		sstatus = "discharging";
	else if (chargingState == CHARGE)
		sstatus = "charging";
	else if (chargingState == EXT_PWR)
		sstatus = "externally powered";

	// prepare tooltip
	sprintf(timeStr, "%s", sstatus);
	if (strcmp(sstatus,"charging") == 0) {
		if ((time > 0) && (time < 1000)) {
			if (time <= 90) {
				sprintf(timeStr, "Charging time: %d minutes\nV=%.2f, mA=%.0f", time, v_average, a_average);
			}
			else {
				sprintf(timeStr, "Charging time: %.1f hours\nV=%.2f, mA=%.0f", (float)time / 60.0, v_average, a_average);  
			}
		}
	}
	else if (strcmp(sstatus,"discharging") == 0) {
		if ((time > 0) && (time < 1000)) {
			if (time <= 90) {
				sprintf(timeStr, "Time remaining: %d minutes\nV=%.2f, mA=%.0f", time, v_average, a_average);
			}
			else {
				sprintf(timeStr, "Time remaining: %.1f hours\nV=%.2f, mA=%.0f", (double)time / 60.0, v_average, a_average);  
			}
		}
	}
	else {
		if (strcmp(sstatus,"externally powered") == 0) {
			sprintf(timeStr, "Fully Charged\nVoltage = %.2f", v_average);
		}
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
			show_warning(popup_window, MainWindow, "Battery EMPTY!", "System Shutting Down in 30s!!");
		}
		if (shut_down_timer <= 0) {
			printf("Battery EMPTY, forcing system shutdown NOW!\n");
			fprintf(logFile,"Battery EMPTY, forcing system shutdown NOW!\n");
			fclose(logFile);
			system("sudo shutdown now");
			gtk_main_quit();
		}
		shut_down_timer -= INTERVAL/1000;
	}
	// Display Battery Low if popup not already active
	if ((capacity <= BATTERY_WARN) && (chargingState == DISCHARGE) && (popup_alarm == FALSE)) {
		if(DEBUG && 1) printf("Battery LOW, please enable exteral power\n");
		popup_alarm = TRUE;
		show_warning(popup_window, MainWindow, "Battery LOW!", "Enable external power supply");
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
		strcat(s, "/RedReactor_batteryLog.txt" );
		// Avoid logfile getting too big with daily use
		logFile = fopen(s,"w");
		fprintf(logFile, "Starting Red Reactor pi-battery-widget\n");
	}
	else
		logFile = stdout;

	// get LXDE panel icon size
	iconSize = -1;
	strcpy(s, homedir);
	strcat(s, "/.config/lxpanel/LXDE-pi/panels/redreactor.conf");
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
		iconSize = 36;
	
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

	global_timeout_ref = g_timeout_add(INTERVAL, (GSourceFunc) timer_event, (gpointer) MainWindow);

	// Call the timer function because we don't want to wait for the first time period triggered call

	clock_gettime(CLOCK_MONOTONIC, &resolution);
	start_time = (resolution.tv_sec); 
	timer_event(MainWindow);

	gtk_main();

	return 0;
}
