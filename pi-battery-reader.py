#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
*** RED REACTOR - Copyright (c) 2022
*** Author: Pascal Herczog

*** This code is designed for the RED REACTOR Raspberry Pi Battery Power Supply
*** Example code provided without warranty
*** Provides Voltage and Current readings to Battery Icon

*** Filename: pi-battery-reader.py
*** PythonVn: 3.8, 32-bit
*** Date: January 2022
"""
from ina219 import INA219  # This controls the battery monitoring IC
from ina219 import DeviceRangeError  # Handle reading errors


# Constants
# RED REACTOR I2C address
I2C_ADDRESS = 0x40

# RED REACTOR Measurement Shunt (defined in Ohms)
SHUNT_OHMS = 0.05

# Set Current Measurement Range
MAX_EXPECTED_AMPS = 5.5

voltage = 0.0
current = 0.0

# ADC Default
# ADC*12BIT: 12 bit, conversion time 532us (default).

# Verify that RED REACTOR is attached, else return defaults
try:
    bat_reader = INA219(SHUNT_OHMS, MAX_EXPECTED_AMPS)
    bat_reader.configure(bat_reader.RANGE_16V)

    voltage = bat_reader.voltage()
    current = bat_reader.current()
except DeviceRangeError as e:
    # Current out of range for the RedReactor
    voltage = 4.5
    current = 6000.0

except OSError:
    """RED REACTOR IS NOT Attached, returning defaults"""
    # 0v means No Red Reactor fitted
    print("0.000|0.00")
    exit(1)

finally:
    # Read by sscanf(buffer, "%f|%f",&vv, &current);
    print("{:.3f}|{:.2f}".format(voltage, current))
