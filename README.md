# pi-battery-widget
Readme for https://github.com/jdolinay/pi-battery-widget

This code has been forked from the Red Factor widget (https://github.com/Scally-H/RedReactor)
To be used as universal widget with INA219 sensor.

**Main changes**
- converted the code to C++ to be able to use INA219 library (https://github.com/regisin/ina219)
- python script not used
- config file renamed to pi-battery-widget.config
- added install scripts for Twister OS (https://twisteros.com/)


**Features**
- Battery LOW Warning Pop-up at 10%
- Auto shutdown 60 seconds after Battery EMPTY Warning Pop-up at 0%
- Displays a battery widget on the desktop panel of the general Raspberry LXDE at the right side
in the System Tray section.
- The green bar turns red if the battery charge left is below 10%, and yellow if the
battery is charging.
- The time remaining is displayed as a tooltip (both for charging and discharging!)
- The display is updated every 10 seconds
- Logs all activities in ~/pi-battery-widget_batteryLog.txt (if enabled)
 
![Alt text](icon.png?raw=true "panel with battery widget")


**Installation for Raspberry Pi OS**

The application assumes you already have python3 installed (usually the case on Raspberry Pi OS).  
However, you also need to install the python INA219 library, as follows from a terminal window:


```
  cd
  cd Downloads
  git clone https://github.com/jdolinay/pi-battery-widget
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

Please check the install script output for errors. It has been tested on Raspberry Pi with Buster,
and creates or updates the ~/.config/lxsession/LXDE-pi/autostart file.

Please adjust your chosen battery capacity by editing ~/.config/lxpanel/LXDE-pi/panels/pi-battery-widget.conf
(given in mAh for total capacity)

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

**Installation for Twister OS**

```
  cd
  cd Downloads
  git clone https://github.com/jdolinay/pi-battery-widget
  cd pi-battery-widget
```

Make sure GTK3.0 library is installed:
```
sudo apt install libgtk-3-dev
```
Make sure wiringpi library is installed: 
```
sudo apt-get install wiringpi
```
For more info see http://wiringpi.com/download-and-install/

Compile the code with the following commands:
```
  make
```

Finally install the widget with the following commands:
```
  chmod +x twister_install
  ./twister_install 
```

Please check the install script output for errors. 
Please adjust your chosen battery capacity by editing ~/.config/xfce4/panel//pi-battery-widget.conf 
(given in mAh for total capacity)

Reboot your pi.


**To uninstall the widget for Twister OS**

Open a terminal and type

```
  cd
  cd Downloads/pi-battery-widget
  chmod +x twister_uninstall
  ./twister_uninstall
```
**Note for TwisterOS**
If the widget is to be able to shutdown the Pi when battery is empty,
you need to enable running sudo shutdown command without prompt for password.
To do this run sudo visudo and add the following line to the sudoers file:

your_user_name ALL=(ALL) NOPASSWD: /sbin/poweroff, /sbin/reboot, /sbin/shutdown

For more into see https://linuxhandbook.com/sudo-without-password/


**New features in the Red Reactor version (from original readme)**

pi-battery-widget status icon widget, with major design changes
to support The Red Reactor Raspberry Pi UPS, including accurate battery life modelling for charging
and discharging profiles.<br>
Now features Battery LOW warning and Battery EMPTY Auto-shutdown.

For details see https://github.com/Scally-H/RedReactor  

**Reference**

https://github.com/jdolinay/pi-battery-widget
https://www.theredreactor.com  
https://github.com/Scally-H/RedReactor  
https://github.com/mezl/pi-battery-widget  
https://github.com/linshuqin329/UPS-18650  
https://github.com/rricharz/pi-top-battery-widget  

