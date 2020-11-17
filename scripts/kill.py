# Copyright (C) strawberryhacker

import os
import sys
import serial

from citrus import citrus_packet
from citrus import citrus_file
from loading import loading_simple
from loading import loading_bar

# Used for loading a custom application to the CitrusOS and execute it

def main():
    if len(sys.argv) != 2:
        print("Check parameters")
        sys.exit()

    pid = int(sys.argv[1])

    # Just open a new COM port
    try:
        s = serial.Serial("/dev/ttyS4", \
            baudrate=921600, timeout=1)

    except serial.SerialException as e:
        print("Cannot open COM port - ", e)
        sys.exit()

    packet = citrus_packet(s)
    pid_bytes = pid.to_bytes(4, byteorder = "little")
    packet.send_packet(pid_bytes, packet.CMD_KILL)

main()
