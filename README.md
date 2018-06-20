# pi-battery-widget

- Displays a battery widget on the desktop panel of the general Raspberry LXDE at the right side
in the System Tray section.
- The green bar turns red if the battery charge left is below 10%, and yellow if the
battery is charging.
- The time remaining is displayed as a tooltip
- The display is updated every 5 seconds
- Logs all activities in ~/batteryLog.txt
- Actuall battery reading code is done in python script, you can swap the script to read differet value

![Alt text](icon.png?raw=true "panel with battery widget")



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
