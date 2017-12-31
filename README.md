# pi-top-battery-widget

- Displays a battery widget on the desktop panel of the pi-top laptop (version 1 and 2) at the right side
in the System Tray section.
- The green bar turns red if the battery charge left is below 10%, and yellow if the
battery is charging.
- The time remaining is displayed as a tooltip
- The display is updated every 5 seconds
- Can be installed in pi-topOS and Raspbian Stretch
- Logs all activities in ~/batteryLog.txt

![Alt text](icon.png?raw=true "panel with battery widget")

>Starting with the latest upgrade of Raspian Stretch (December 1, 2017) a battery icon
>should be automatically installed and enabled on the pi-top rev 1.
>The apt package *lxplug-ptbatt* is part of the apt package *raspberry-pi-ui-mods*.
>If this battery icon shows up on your pi-top, installation of the battery widget in this
>repository is not required anymore.

If you want to use this battery widget with Raspian Stretch, you first need to make sure
that the latest pi-top software for the pi-top hardware is installed. More
information can be found in the [pi-top-setup repository](http:github.com/rricharz/pi-top-setup). 
On pi-topOS you can skip this step. Open a terminal and type


```
sudo apt update
sudo apt upgrade
sudo apt install pt-hub
```

Now you can check whether pt-battery works properly on your pi-top.
Open a terminal and type:

```
  pt-battery
```

The widget will work properly if the output looks as follows (of course with different values
for each parameter)

```
Charging State: 0
Capacity: 70
Time Remaining: 590
Wattage: 0
```

If pt-battery does not work properly (it constantly displays 50% due to a bug), you need to check
i2c and spi in Menu->Preferences->Raspberry Pi Configuration. On the pi-top version 1, i2c and spi
need to be turned on, whereas on the pi-top version 2 turning i2c on is sufficient.

If you have been using
[pi-top-battery-status](http://github.com/rricharz/pi-top-battery-status) before,
you should uninstall it. See the instructions to uninstall that program in the description
of that repository. The low power warnings and the automatic shutdown are now part of the
pi-top software.


**Installation**

Open a terminal and type the following

```
  cd
  cd Downloads
  git clone --depth 1 git://github.com/rricharz/pi-top-battery-widget
  cd pi-top-battery-widget
```
The following step is only required, if you want to install pi-top-battery-widget
on pi-topOS or Raspbian Jessie. Skip this step if you are using Raspbian Stretch.
```
cd version-for-jessie
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
  cd Downloads/pi-top-battery-widget
  git pull
```
The following step is only required, if you want to update pi-top-battery-widget
on pi-topOS or Raspbian Jessie. Skip this step if you are using Raspbian Stretch.
```
cd version-for-jessie
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
  cd Downloads/pi-top-battery-widget
  chmod +x uninstall
  ./uninstall
```


**What to do if bluetooth does not work after installing pt-pulse**

See the workaround at the bottom of [pi-topPULSE](https://github.com/pi-top/pi-topPULSE).


Please open an issue in this repository or write to rricharz77@gmail.com if you have any feedback
or problem with this repository. Your input is appreciated.