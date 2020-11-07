# Copyright (C) strawberryhacker

import serial
import argparse
import math
import sys
import time

try:
    com = serial.Serial("/dev/ttyS4", baudrate=576000, timeout=1)
except serial.SerialException as e:
    print(e)
    exit()
print(len("hello this is a little test to see if the DMA work"))
com.write(b'hello this is a little test to see if the DMA work')
