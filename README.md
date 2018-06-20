# pi-battery-widget

I was looking a way to display battery icon with my Chinese 18650 battery pack.
I found pi-top-battery-widget is what I am looking for. So I fork the code and adapted to display
the UPS-18650 using I2C reading.

Instead reading the battery info directly in the c code, I choose using pi-battery-reader.py which I 
adapted from UPS-18650 sample code. This way make others much easier to using their own method to read 
battery info from other method(e.g I2C,SPI,USB,UART).

The C code whihc execute python code to read battery values.
Python code out put format is 
```
voltage(float) | capacity(int) | chargingState(int) 

5.2 | 89 | 1
```
chargingState = 0 is "discharging"
chargingState = 1 is "charging"
chargingState = 2 is "AC or Externally Powered "
 

- Displays a battery widget on the desktop panel of the general Raspberry LXDE at the right side
in the System Tray section.
- The green bar turns red if the battery charge left is below 10%, and yellow if the
battery is charging.
- The time remaining is displayed as a tooltip
- The display is updated every 5 seconds
- Logs all activities in ~/batteryLog.txt
- Actuall battery reading code is done in python script, you can swap the script to read differet value

![Alt text](icon.png?raw=true "panel with battery widget")

![Alt text](UPS-18650.png "Chinese 18650 battery Pi UPS" =100x)



**Installation**

Open a terminal and type the following

```
  cd
  cd Downloads
  git clone --depth 1 git://github.com/mezl/pi-battery-widget
  cd pi-battery-widget
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

Please open an issue in this repository or write to mezlxx@gmail.com if you have any feedback
or problem with this repository. Your input is appreciated.


**Reference**
https://github.com/linshuqin329/UPS-18650
https://github.com/rricharz/pi-top-battery-widget

