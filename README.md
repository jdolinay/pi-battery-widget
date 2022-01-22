# pi-battery-widget

This code has been forked from the pi-battery-widget status icon widget, with major design changes
to support The Red Reactor Raspberry Pi UPS, including accurate battery modelling for charging and
discharging profiles.

The C code which execute python code to read battery values.
Python code out put format is 
```
voltage(float) | current(float) 

4.2 | 1088.9
```
chargingState = -1 is "no battery"
chargingState = 0 is "discharging"  
chargingState = 1 is "charging"  
chargingState = 2 is "AC or Externally Powered "  
 


**Features**
- Displays a battery widget on the desktop panel of the general Raspberry LXDE at the right side
in the System Tray section.
- The green bar turns red if the battery charge left is below 10%, and yellow if the
battery is charging.
- The time remaining is displayed as a tooltip
- The display is updated every 5 seconds
- Logs all activities in ~/RedReactor_batteryLog.txt
- Actuall battery reading code is done in python script based on the Red Reactor configuration

![Alt text](icon.png?raw=true "panel with battery widget")

<img src="UPS-18650.png" width="50%"  alt="The Red Reactor Raspberry Pi 18650 UPS">
The Red Reactor UPS for Raspberry Pi zero, Pi Model 2/3 and Pi Model 4!






**Installation**

Open a terminal and type the following

```
  cd
  cd Downloads
  git clone https://github.com/mezl/pi-battery-widget.git
  cd pi-battery-widget
```

Make sure GTK3.0 library is installed:
```
sudo apt install libgtk-3-dev
```


Compile the code with the following commands:
```
  make
```

Finally install the widget with the following commands:
```
  chmod +x install
  ./install 
```

Reboot your pi.


**To update the widget to the latest version**

Open a terminal and type

```
  cd
  cd Downloads/pi-battery-widget
  git pull
  make
```
Now install the updated widget
```
  chmod +x install
  ./install
```


**To uninstall the widget**

Open a terminal and type

```
  cd
  cd Downloads/pi-battery-widget
  chmod +x uninstall
  ./uninstall
```

Please open an issue in this repository or write to hello@theredreactor if you have any feedback
or problem with this repository. Your input is appreciated.


**Reference**

https://github.com/linshuqin329/UPS-18650  
https://github.com/rricharz/pi-top-battery-widget  

