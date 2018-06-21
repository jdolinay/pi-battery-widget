/*
 * pi-battery-widget.c
 * 
 * display raspberry pi battery status
 * gets information from the I2C(another python script)
 *
 * Copyright 2018  Chin-Kai Chang <mezlxx@gmail.com>
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

#define MAKELOG         1     // log file batteryLog in home directory (0 = no log file)
#define MAX_CHARGED_VOLTAGE 4.2
#define CUTOFF_VOLTAGE 2.4

//for rough charge/dischare time estimation
#define CELL_CAPACITY 3400 //mAh
#define CELL_NUMS 2
#define TOTAL_CAPACITY CELL_CAPACITY*CELL_NUMS
#define CHARGING_CURRENT 500 //ma
#define DISCHARGING_CURRENT 1000 //ma
#define DEBUG TRUE 

const int TOTAL_CHARGE_TIME = TOTAL_CAPACITY * 60 / CHARGING_CURRENT; //816 min
const int TOTAL_DISCHARGE_TIME = TOTAL_CAPACITY * 60 / DISCHARGING_CURRENT; //408 min

enum State{NO_BATT = -1,DISCHARGE = 0,CHARGE = 1, EXT_PWR = 2};
cairo_surface_t *surface;
gint width;
gint height;

GtkWidget *MainWindow;

guint global_timeout_ref;
int first;
GtkStatusIcon* statusIcon;
FILE *logFile;

int last_capacity, last_state, last_time;
double last_voltage;
double start_time; 
struct timespec resolution;

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
    /* Read the output a line at a time - output it. */
    while (fgets(s, sizeof(s)-1, fp) != NULL) {
        //printf("RAW[%s]", s);
        strcpy(buffer, s);
    }

    /* close */
    pclose(fp);

    return 0;

}

void printLogEntry(int capacity, char* state, char* timeStr,double voltage) {
    time_t rawtime;
    struct tm *timeinfo;
    char timeString[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timeString, 80, "%D %R:%S", timeinfo);
    fprintf(logFile,"%s", timeString);
    // printf("%s:  %s %d\n", timeString, s, i);

    fprintf(logFile, ", Capacity: %3d%%", capacity);

    fprintf(logFile, ", %s, %s", state, timeStr);
    fprintf(logFile, ",voltage %7.5fV\n",voltage);
    fflush(logFile);
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
    char *sstatus;
    int response; 
    float wattage;

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
        exec (s,buffer); 
        sscanf(buffer, "%f|%i|%d|%f",&vv, &capacity,&chargingState,&wattage);
        response = 1;
        voltage = (double)vv;
        if(last_voltage < 0)last_voltage = voltage;

        chargingState = last_state;//overwrite from python reading
        //using voltage to determine the chargingState
        if(voltage <= 0.0){
            chargingState = NO_BATT; //no battery
            printf("No BATT:");
        } else if (last_voltage > voltage){
            chargingState = DISCHARGE; //discharging

            double voltage_diff = (last_voltage - voltage);
            printf("\nV: %f, Vlast: %f",voltage,last_voltage);
            printf("Vd %f\n",voltage_diff);

            double diff2 = timer_me(TRUE);
            printf("Time: %f, Vd: %f\n",diff2,voltage_diff);

            int time1 = TOTAL_CHARGE_TIME*(100-capacity)/100;//TODO , this only assume 500mAh charge current 

            double remaining_voltage = voltage - CUTOFF_VOLTAGE;
            double remaining_time = (remaining_voltage / voltage_diff) * diff2 /60;
            int time2 = (int)remaining_time; //in minute

            printf("M1: %d min, M2: %d min\n",time1,time2);

            time = time2; //use method 2 to estimate time

            printf("Discharging:");

        }else if(last_voltage < voltage){
            chargingState = CHARGE; //charging

            double voltage_diff = (voltage- last_voltage);

            printf("\nV: %f, Vlast: %f",voltage,last_voltage);
            printf("Vd %f\n",voltage_diff);
            
            double diff2 = timer_me(TRUE);
            printf("Time: %f, Vd: %f\n",diff2,voltage_diff);

            //constant current method
            int time1 = TOTAL_DISCHARGE_TIME * capacity/100 ;//TODO, this only assume 1A discharge current 
            
            //voltage drop method
            //since we know it taks 52 sec to increase 0.00125V
            double remaining_voltage = MAX_CHARGED_VOLTAGE - voltage;
            double remaining_time = (remaining_voltage / voltage_diff) * diff2 /60.0;
            int time2 = (int)remaining_time; // in minutes
            printf("M1: %d min, M2: %d min\n",time1,time2); 

            time = time2; //use method 2 to estimate time
            printf("Charging:");

        }else{
            if(voltage >= MAX_CHARGED_VOLTAGE){
                chargingState = EXT_PWR;//externally powered and fully charged
                printf("EXT_PWR:");
            }


            //double diff2 = timer_me(FALSE);
            //printf("\nTimer: %f\n",diff2);

            printf("Default:");
            time = last_time;
        }

        printf("Remaining time is %d min,voltage %7.5fV\n",time,voltage);

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

    // printf("Charging State: %d, ", chargingState);
    // printf("Capacity: %d\n", capacity);

    if ((capacity > 100) || (capacity < 0))
        capacity = -1;              // capacity out of limits


    // check whether state or capacity has changed
    if ((last_state == chargingState) && (last_capacity == capacity) && (last_voltage == voltage)) {
        // restart timer and return without updating icon
        global_timeout_ref = g_timeout_add(INTERVAL, (GSourceFunc) timer_event, (gpointer) MainWindow);	
        if(DEBUG && 0) printf("state does not change, no update\n");
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
                sprintf(timeStr, "Charging time: %d minutes", time);
            }
            else {
                sprintf(timeStr, "Charging time: %.1f hours", (float)time / 60.0);  
            }
        }
    }
    else if (strcmp(sstatus,"discharging") == 0) {
        if ((time > 0) && (time < 1000)) {
            if (time <= 90) {
                sprintf(timeStr, "Time remaining: %d minutes", time);
            }
            else {
                sprintf(timeStr, "Time remaining: %.1f hours", (double)time / 60.0);  
            }
        } 
    }

    // update log

    printLogEntry(capacity, sstatus, timeStr,voltage);

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
        cairo_set_source_rgb (cr, 1, 1, 0);
    else if (capacity <= REDLEVEL)
        cairo_set_source_rgb (cr, 1, 0, 0);
    else if (strcmp(sstatus,"externally powered") == 0)
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
        strcat(s, "/batteryLog.txt" );
        // printf("s = %s\n",s);
        logFile = fopen(s,"a");
        fprintf(logFile, "Starting pi-battery-widget\n");
    }
    else
        logFile = stdout;

    // get lxpanel icon size
    iconSize = -1;
    strcpy(s, homedir);
    strcat(s, "/.config/lxpanel/Lubuntu/panels/panel");
    FILE* lxpanel = fopen(s, "r");
    if (lxpanel == NULL) {
        printf("Failed to open lxpanel config file %s\n", s);
        fprintf(logFile,"Failed to open lxpanel config file %s\n", s);
    }
    else {
        char lxpaneldata[2048];
        while ((fgets(lxpaneldata, 2047, lxpanel) != NULL) && (iconSize == -1)) {
            sscanf(lxpaneldata,"  iconsize=%d", &iconSize);
        }
        fclose(lxpanel);
    }
    if (iconSize == -1)    // default
        iconSize = 36;
    // printf("lxpanel iconSize = %d\n", iconSize);

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
