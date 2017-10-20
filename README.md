# pi-top-battery-widget

- Displays a battery widget on the desktop panel of the pi-top laptop at the right side
in the System Tray section.
- The green bar turns red if the battery charge left is below 10%, and yellow if the
battery is charging.

> The widget uses the pt-battery script to obtain the necessary information about the battery.
> Currently, this script is only installed and running on pi-topOS.
> It has been tested on the original pi-top, but is expected to work properly
> on the new pi-top with the sliding keyboard

Before installing the widget, make sure that pt-battery works properly on your pi-top.
Open a terminal and type:

```
  pt-battery
```

You can install the widget if the output looks as follows (of course with different values
for each parameter)

```
Charging State: 0
Capacity: 70
Time Remaining: 590
Wattage: 0
```

**Installation**

Open a terminal and type the following

```
  cd
  cd Downloads
  git clone --depth 1 git://github.com/rricharz/pi-top-battery-widget
  cd pi-top-battery-widget
  chmod +x install
  ./install 
```

Reboot your pi.

**To uninstall the widget  **

Open a terminal and type

```
  cd
  cd Downloads/pi-top-battery-widget
  chmod +x ./uninstall
  ./uninstall
```
